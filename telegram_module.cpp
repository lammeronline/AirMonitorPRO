#include "telegram_module.h"
#include "config.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace TelegramModule {

static bool   _enabled  = false;
static String _token;
static String _chatId;

// -- Alert thresholds — loaded from NVS ------------------─
static uint16_t _thr_co2     = ALERT_DEFAULT_CO2_PPM;
static uint16_t _thr_tvoc    = ALERT_DEFAULT_TVOC_PPB;
static uint8_t  _thr_aqi     = ALERT_DEFAULT_AQI;
static float    _thr_temp_hi = ALERT_DEFAULT_TEMP_HI;
static float    _thr_hum_hi  = ALERT_DEFAULT_HUM_HI;
static uint8_t  _thr_cooldown_min = ALERT_DEFAULT_COOLDOWN_MIN;

static int64_t  _lastUpdateId   = 0;
static uint32_t _lastPoll       = 0;
static uint32_t _lastAlertCO2   = 0;
static uint32_t _lastAlertTVOC  = 0;
static uint32_t _lastAlertAQI   = 0;
static uint32_t _lastAlertTemp  = 0;
static uint32_t _lastAlertHum   = 0;

static const uint32_t POLL_INTERVAL         =   5000UL;
static const uint32_t WIFI_STABLE_MS        =   5000UL;
static const uint32_t API_FAIL_COOLDOWN_MS  =  15000UL;
static const uint32_t TLS_LOG_INTERVAL_MS   =  30000UL;
static const uint32_t API_READ_TIMEOUT_MS   =   1200UL;
static const uint32_t API_TOTAL_TIMEOUT_MS  =   2500UL;
static const uint32_t API_HEALTH_LOG_MS     =  60000UL;

static uint32_t _nextApiAttempt   = 0;
static uint32_t _lastTlsFailLog   = 0;
static bool     _startupMsgSent   = false;
static uint32_t _lastApiMs        = 0;
static uint32_t _apiFailCount     = 0;
static uint32_t _lastApiHealthLog = 0;

// -- Startup protection flag --
static bool     _firstCommandProcessed = false;  // Set after first command is handled

// -- /reboot DoS protection -------
static uint32_t _lastRebootCmdMs = 0;
static const uint32_t REBOOT_COOLDOWN_MS = 60000;  // 60 sec between reboots

// -- FreeRTOS --------------------------------------------─
static TaskHandle_t     _tgTaskHandle = NULL;
static SemaphoreHandle_t _dataMutex   = NULL;
static SensorData        _currentData;
static SystemStatus      _currentStatus;

static bool _wifiReady() {
    return WifiManager::isStableConnected(WIFI_STABLE_MS);
}

// --------------------------------------------------------─
static void _loadPrefs() {
    Preferences p;
    p.begin("airmon", true);
    _enabled      = p.getBool  ("tg_en",       false);
    _token        = p.getString("tg_token",    "");
    _chatId       = p.getString("tg_chatid",   "");
    _thr_co2      = (uint16_t)p.getInt  ("thr_co2",   ALERT_DEFAULT_CO2_PPM);
    _thr_tvoc     = (uint16_t)p.getInt  ("thr_tvoc",  ALERT_DEFAULT_TVOC_PPB);
    _thr_aqi      = (uint8_t) p.getInt  ("thr_aqi",   ALERT_DEFAULT_AQI);
    _thr_temp_hi  = p.getFloat("thr_temp_hi",  ALERT_DEFAULT_TEMP_HI);
    _thr_hum_hi       = p.getFloat("thr_hum_hi",    ALERT_DEFAULT_HUM_HI);
    _thr_cooldown_min = (uint8_t)p.getInt("thr_cooldown", ALERT_DEFAULT_COOLDOWN_MIN);
    // Load last processed update_id to avoid re-processing old commands after reboot
    // Preferences doesn't support getInt64, so store as string
    String lastUidStr = p.getString("tg_lastuid", "0");
    _lastUpdateId = strtoll(lastUidStr.c_str(), NULL, 10);
    p.end();
}

// -- Save last processed update ID (to prevent duplicate command execution) -----
static void _saveLastUpdateId() {
    Preferences p;
    p.begin("airmon", false);
    // Preferences doesn't support putInt64, so convert to string
    p.putString("tg_lastuid", String(_lastUpdateId));
    p.end();
}

// -- Save thresholds (called from web_server_module) ------─
void saveThresholds(uint16_t co2, uint16_t tvoc, uint8_t aqi,
                    float tHi, float hHi, uint8_t cooldownMin) {
    _thr_co2          = co2;
    _thr_tvoc         = tvoc;
    _thr_aqi          = aqi;
    _thr_temp_hi      = tHi;
    _thr_hum_hi       = hHi;
    _thr_cooldown_min = cooldownMin;
    Preferences p;
    p.begin("airmon", false);
    p.putInt  ("thr_co2",      co2);
    p.putInt  ("thr_tvoc",     tvoc);
    p.putInt  ("thr_aqi",      aqi);
    p.putFloat("thr_temp_hi",  tHi);
    p.putFloat("thr_hum_hi",   hHi);
    p.putInt  ("thr_cooldown", cooldownMin);
    p.end();
    DBGF("[TG] Thresholds saved CO2=%d TVOC=%d AQI=%d T>%.1f H>%.1f cooldown=%dmin\n",
         co2, tvoc, aqi, tHi, hHi, cooldownMin);
}

ThresholdConfig getThresholds() {
    return {_thr_co2, _thr_tvoc, _thr_aqi, _thr_temp_hi, _thr_hum_hi, _thr_cooldown_min};
}

// --------------------------------------------------------─
static String _apiPost(const String& method, const String& body) {
    uint32_t now = millis();
    if (!_wifiReady() || _token.isEmpty()) return "";
    if (_nextApiAttempt && now < _nextApiAttempt) return "";

    WiFiClientSecure tls;
    tls.setInsecure();
    tls.setTimeout(API_READ_TIMEOUT_MS);

    uint32_t t0 = millis();
    if (!tls.connect("api.telegram.org", 443)) {
        _lastApiMs = millis() - t0;
        _apiFailCount++;
        if (now - _lastTlsFailLog >= TLS_LOG_INTERVAL_MS) {
            _lastTlsFailLog = now;
            DBGF("[TG] TLS connect failed (%ums)\n", (unsigned)_lastApiMs);
        }
        _nextApiAttempt = now + API_FAIL_COOLDOWN_MS;
        return "";
    }
    _nextApiAttempt = 0;

    String url = "/bot" + _token + "/" + method;
    String req = "POST " + url + " HTTP/1.1\r\n"
                 "Host: api.telegram.org\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: " + body.length() + "\r\n"
                 "Connection: close\r\n\r\n" + body;
    tls.print(req);

    String resp = "";
    bool   inHeaders = true;
    int    lineCount = 0;
    while ((tls.connected() || tls.available()) &&
           (millis() - t0 < API_TOTAL_TIMEOUT_MS)) {
        if (tls.available()) {
            String line = tls.readStringUntil('\n');
            // Remove trailing \r if present
            if (line.endsWith("\r")) {
                line.remove(line.length() - 1);
            }
            
            if (inHeaders) {
                if (line.length() == 0) {  // Empty line = end of headers
                    inHeaders = false;
                }
            } else {
                resp += line;
            }
            lineCount++;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    _lastApiMs = millis() - t0;
    tls.stop();
    
    // Only log if response is empty or error
    if (resp.isEmpty() || resp.length() < 20) {
        DBGF("[TG] %s: %ums, bodyLen=%u\n", method.c_str(), (unsigned)_lastApiMs, (unsigned)resp.length());
    }
    return resp;
}

// --------------------------------------------------------─
bool sendMessage(const String& text) {
    if (!_enabled || _token.isEmpty() || _chatId.isEmpty()) return false;
    DynamicJsonDocument body(1024);
    body["chat_id"]    = _chatId;
    body["text"]       = text;
    body["parse_mode"] = "HTML";
    String bodyStr; serializeJson(body, bodyStr);
    String resp = _apiPost("sendMessage", bodyStr);
    bool ok = resp.indexOf("\"ok\":true") >= 0;
    DBGF("[TG] sendMessage: %s\n", ok ? "OK" : "FAIL");
    return ok;
}

static String _buildStatus(const SensorData& d, const SystemStatus& s) {
    const char* aqiEmoji = d.aqi<=1?"🟢":d.aqi==2?"🟡":d.aqi==3?"🟠":d.aqi==4?"🔴":"🟣";
    String msg = "<b>🌿 Air Monitor PRO</b>\n-----------\n";
    msg += "🌡 Температура: <b>" + String(d.temp,1) + " °C</b>\n";
    msg += "💧 Влажность:   <b>" + String(d.hum,1)  + " %</b>\n";
    msg += "🫁 CO₂:         <b>" + String(d.co2)    + " ppm</b>\n";
    msg += "🧪 TVOC:        <b>" + String(d.tvoc)   + " ppb</b>\n";
    msg += String(aqiEmoji)+" AQI: <b>"+String(d.aqi)+"/5</b>\n";
    msg += "-----------\n";
    msg += "📡 IP: " + s.ip + "\n";
    msg += "🕐 " + s.date_str + " " + s.time_str + "\n";
    msg += "⏱ Uptime: "+String(s.uptime/3600)+"h "+String((s.uptime%3600)/60)+"m\n";
    msg += "-----------\n<i>Пороги: CO₂&gt;"+String(_thr_co2)+
           " TVOC&gt;"+String(_thr_tvoc)+
           " AQI≥"+String(_thr_aqi)+
           " T&gt;"+String(_thr_temp_hi,1)+
           " RH&gt;"+String(_thr_hum_hi,1)+"</i>";
    return msg;
}

static void _pollUpdates(const SensorData& d, const SystemStatus& s) {
    DynamicJsonDocument req(128);
    req["offset"]  = _lastUpdateId + 1;
    req["limit"]   = 5;
    req["timeout"] = 0;
    String reqStr; serializeJson(req, reqStr);
    String resp = _apiPost("getUpdates", reqStr);
    if (resp.isEmpty()) {
        DBGLN("[TG] getUpdates response empty");
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, resp);
    if (err != DeserializationError::Ok) {
        DBGF("[TG] JSON parse error: %s (respLen=%u, first100: '%.100s')\n", 
             err.c_str(), (unsigned)resp.length(), resp.c_str());
        return;
    }
    if (!doc["ok"].as<bool>()) {
        DBGLN("[TG] Telegram API error (ok!=true)");
        return;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    if (results.size() > 0) {
        DBGF("[TG] Got %u updates\n", (unsigned)results.size());
    }
    
    // If this is first poll after startup, skip old commands that were queued before reboot
    if (!_firstCommandProcessed && results.size() > 0 && millis() < 30000) {
        int64_t lastUID = results[results.size() - 1]["update_id"].as<int64_t>();
        DBGF("[TG] FIRST POLL: skipping %u queued updates (max uid=%lld)\n", 
             (unsigned)results.size(), lastUID);
        _lastUpdateId = lastUID;
        _firstCommandProcessed = true;
        _saveLastUpdateId();
        return;  // Don't process any commands on first poll
    }
    _firstCommandProcessed = true;  // Mark that we've processed at least one poll
    
    for (JsonObject upd : results) {
        int64_t uid = upd["update_id"].as<int64_t>();
        // Only process if this is a new update_id we haven't seen before
        if (uid <= _lastUpdateId) {
            continue;  // Silently skip old updates
        }
        
        // JSON VALIDATION: Check structure before accessing fields
        if (!upd.containsKey("message") || !upd["message"].is<JsonObject>()) {
            _lastUpdateId = uid;  // Mark as processed anyway
            continue;  // Silently skip malformed
        }
        
        JsonObject msg = upd["message"].as<JsonObject>();
        if (!msg.containsKey("chat") || !msg["chat"].is<JsonObject>() || !msg.containsKey("text")) {
            _lastUpdateId = uid;
            continue;  // Silently skip incomplete message
        }
        
        // Now safe to extract fields
        String chatId = msg["chat"]["id"].as<String>();
        String text   = msg["text"].as<String>();
        
        // IMPORTANT: update _lastUpdateId even before processing command
        // This ensures that even ignored commands won't be re-processed after reboot
        _lastUpdateId = uid;
        
        if (chatId != _chatId) {
            continue;  // Silently skip wrong chat
        }

        if (text=="/status"||text=="/start") {
            DBGLN("[TG] Handling /status");
            sendMessage(_buildStatus(d,s));
        }
        else if (text=="/report") {
            DBGLN("[TG] Handling /report");
            DynamicJsonDocument jd(256);
            jd["temp"]=d.temp; jd["hum"]=d.hum; jd["co2"]=d.co2;
            jd["tvoc"]=d.tvoc; jd["aqi"]=d.aqi; jd["time"]=s.time_str;
            String jstr; serializeJson(jd,jstr);
            sendMessage("<pre>"+jstr+"</pre>");
        }
        else if (text=="/thresholds") {
            DBGLN("[TG] Handling /thresholds");
            sendMessage("⚙️ <b>Текущие пороги алертов:</b>\n"
                        "CO₂ &gt; "         + String(_thr_co2)       + " ppm\n"
                        "TVOC &gt; "        + String(_thr_tvoc)      + " ppb\n"
                        "AQI ≥ "            + String(_thr_aqi)       + "\n"
                        "Температура &gt; " + String(_thr_temp_hi,1) + " °C\n"
                        "Влажность &gt; "   + String(_thr_hum_hi,1)  + " %\n\n"
                        "<i>Изменить: Settings → Alerts</i>");
        }
        else if (text=="/reboot") {
            // DoS protection: allow /reboot only once per 60 seconds
            uint32_t now = millis();
            if (now - _lastRebootCmdMs < REBOOT_COOLDOWN_MS) {
                uint32_t remainMs = REBOOT_COOLDOWN_MS - (now - _lastRebootCmdMs);
                DBGF("[TG] /reboot blocked (cooldown): wait %ld ms\n", remainMs);
                sendMessage("⏱ <b>/reboot</b> был выполнен недавно\n"
                           "Подождите " + String(remainMs/1000) + " сек");
                continue;
            }
            _lastRebootCmdMs = now;
            DBGLN("[TG] Handling /reboot");
            sendMessage("🔄 Перезагружаюсь...");
            delay(2000);
            ESP.restart();
        }
        else if (text=="/help") {
            DBGLN("[TG] Handling /help");
            sendMessage("<b>Команды:</b>\n"
                        "/status — показатели\n/report — JSON\n"
                        "/thresholds — пороги алертов\n"
                        "/reboot — перезагрузка\n/help — справка");
        }
        else {
            DBGF("[TG] Unknown command: %s\n", text.c_str());
        }
    }
    // Save last processed update_id to prevent duplicate command execution after reboot
    if (results.size() > 0) {
        _saveLastUpdateId();
    }
}

static bool _sendAlertIfDue(const SensorData& d, uint32_t now) {
    if (d.co2 > _thr_co2 && (now - _lastAlertCO2   > (uint32_t)_thr_cooldown_min * 60000UL)) {
        if (sendMessage("⚠️ <b>Высокий CO₂!</b>\nТекущий: <b>"+String(d.co2)+
                        " ppm</b>\nПорог: "+String(_thr_co2)+" ppm\n\nПроветрите!")) {
            _lastAlertCO2 = now;
        }
        return true;
    }
    if (d.tvoc > _thr_tvoc && (now - _lastAlertTVOC  > (uint32_t)_thr_cooldown_min * 60000UL)) {
        if (sendMessage("🧪 <b>Высокий TVOC!</b>\nТекущий: <b>"+String(d.tvoc)+
                        " ppb</b>\nПорог: "+String(_thr_tvoc)+" ppb")) {
            _lastAlertTVOC = now;
        }
        return true;
    }
    if (d.aqi >= _thr_aqi && (now - _lastAlertAQI   > (uint32_t)_thr_cooldown_min * 60000UL)) {
        const char* lvl = d.aqi==3?"умеренный":d.aqi==4?"плохой":"опасный";
        if (sendMessage("🔴 <b>Плохой воздух!</b>\nAQI: <b>"+String(d.aqi)+
                        "/5 ("+lvl+")</b>\nTVOC: "+String(d.tvoc)+" ppb")) {
            _lastAlertAQI = now;
        }
        return true;
    }
    if (d.temp > _thr_temp_hi && (now - _lastAlertTemp  > (uint32_t)_thr_cooldown_min * 60000UL)) {
        if (sendMessage("🌡 <b>Высокая температура!</b>\n<b>"+
                        String(d.temp,1)+" °C</b> (порог: "+
                        String(_thr_temp_hi,1)+" °C)")) {
            _lastAlertTemp = now;
        }
        return true;
    }
    if (d.hum > _thr_hum_hi && (now - _lastAlertHum   > (uint32_t)_thr_cooldown_min * 60000UL)) {
        if (sendMessage("💧 <b>Высокая влажность!</b>\n<b>"+
                        String(d.hum,1)+" %</b> (порог: "+
                        String(_thr_hum_hi,1)+" %)")) {
            _lastAlertHum = now;
        }
        return true;
    }
    return false;
}

// -- FreeRTOS Telegram Background Task --------------------
static void _telegramTask(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!_enabled || !WiFi.isConnected() || !_wifiReady()) {
            if (_enabled) {
                DBGF("[TG] Waiting: enabled=%d wifi=%d wifiReady=%d\n",
                     (int)_enabled, WiFi.isConnected(), _wifiReady());
            }
            continue;
        }

        uint32_t now = millis();

        SensorData d;
        SystemStatus s;
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            d = _currentData;
            s = _currentStatus;
            xSemaphoreGive(_dataMutex);
        } else {
            DBGLN("[TG] Mutex timeout");
            continue;
        }

        // Only log health if there are failures or issues
        if (_apiFailCount > 0 && (now - _lastApiHealthLog >= 120000)) {  // Log every 2 min if failures
            _lastApiHealthLog = now;
            DBGF("[TG] API issues: %u failures, cooldown=%s\n",
                 (unsigned)_apiFailCount,
                 (_nextApiAttempt && now < _nextApiAttempt) ? "yes" : "no");
        }

        if (!_startupMsgSent) {
            DBGLN("[TG] Sending startup message...");
            if (sendMessage("🟢 <b>Air Monitor PRO</b> запущен!\nIP: " +
                            WiFi.localIP().toString() +
                            "\nПороги: CO₂&gt;"+String(_thr_co2)+
                            " TVOC&gt;"+String(_thr_tvoc)+
                            " AQI≥"+String(_thr_aqi)+
                            "\nКулдаун: "+String(_thr_cooldown_min)+" мин")) {
                _startupMsgSent = true;
                DBGLN("[TG] Startup message sent");
            } else {
                DBGLN("[TG] Failed to send startup message");
            }
            continue;
        }

        if (_sendAlertIfDue(d, now)) continue;

        if (now - _lastPoll >= POLL_INTERVAL) {
            _lastPoll = millis();
            DBGF("[TG] Polling updates (offset=%lld)...\n", _lastUpdateId);
            _pollUpdates(d, s);
        }
    }
}

// ---- One-time initialization (called in setup) ----
void init() {
    // Load settings from NVS so isEnabled() returns correct value
    _loadPrefs();
    DBGF("[TG] init() called: _enabled=%d token=%s chatid=%s _lastUpdateId=%lld\n",
         (int)_enabled, _token.isEmpty() ? "EMPTY" : "SET", _chatId.isEmpty() ? "EMPTY" : "SET", _lastUpdateId);
    
    // Ensure _lastUpdateId is saved so it persists across reboots
    // This prevents re-processing old commands from Telegram queue
    if (_lastUpdateId > 0) {
        _saveLastUpdateId();
        DBGF("[TG] Saved _lastUpdateId=%lld to NVS\n", _lastUpdateId);
    }
}

// --------------------------------------------------------─
void begin() {
    // _enabled should already be loaded from init(), but load again to be safe
    _loadPrefs();
    if (!_enabled || _token.isEmpty() || _chatId.isEmpty()) {
        DBGLN("[TG] Disabled or not configured");
        _enabled = false;
        return;
    }

    _firstCommandProcessed = false;  // Reset for this startup
    DBGF("[TG] Ready chat_id=%s CO2thr=%d TVOCthr=%d AQIthr=%d\n",
         _chatId.c_str(), _thr_co2, _thr_tvoc, _thr_aqi);

    if (_dataMutex == NULL) {
        _dataMutex = xSemaphoreCreateMutex();
    }

    if (_tgTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            _telegramTask, "TgTask", 10240, NULL, 1, &_tgTaskHandle, 1
        );
    }
}

void reload() {
    DBGLN("[TG] Reloading settings...");
    _loadPrefs();
    DBGF("[TG] After reload: _enabled=%d token=%s chatid=%s\n",
         (int)_enabled, _token.isEmpty() ? "EMPTY" : "SET", _chatId.isEmpty() ? "EMPTY" : "SET");
    if (!_enabled || _token.isEmpty() || _chatId.isEmpty()) {
        DBGLN("[TG] Now disabled or not configured");
        _enabled = false;
        return;
    }
    // Reset state so telegram task will restart polling and send startup message
    _startupMsgSent = false;
    _firstCommandProcessed = false;  // Reset for next startup sequence
    // DO NOT reset _lastUpdateId - load from NVS to prevent re-triggering old commands
    // _lastUpdateId already loaded from NVS in _loadPrefs()
    _lastPoll       = 0;
    _lastAlertCO2   = 0;
    _lastAlertTVOC  = 0;
    _lastAlertAQI   = 0;
    _lastAlertTemp  = 0;
    _lastAlertHum   = 0;
    DBGF("[TG] Reloaded with chat_id=%s _lastUpdateId=%lld\n", _chatId.c_str(), _lastUpdateId);
}

void loop(const SensorData& d, const SystemStatus& s) {
    if (!_enabled || _dataMutex == NULL) return;
    if (xSemaphoreTake(_dataMutex, 0) == pdTRUE) {
        _currentData   = d;
        _currentStatus = s;
        xSemaphoreGive(_dataMutex);
    }
}

bool isEnabled() { return _enabled; }

} // namespace TelegramModule
