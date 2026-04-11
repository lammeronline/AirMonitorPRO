#include "telegram_module.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>

namespace TelegramModule {

static bool   _enabled  = false;
static String _token;
static String _chatId;

// ── Alert thresholds — loaded from NVS ───────────────────
static uint16_t _thr_co2  = 1500;   // ppm
static uint8_t  _thr_aqi  = 3;      // 1-5
static float    _thr_temp_hi = 30.0f; // °C
static float    _thr_hum_hi  = 75.0f; // %

static int64_t  _lastUpdateId  = 0;
static uint32_t _lastPoll      = 0;
static uint32_t _lastAlertCO2  = 0;
static uint32_t _lastAlertAQI  = 0;
static uint32_t _lastAlertTemp = 0;
static uint32_t _lastAlertHum  = 0;

static const uint32_t POLL_INTERVAL  = 5000UL;
static const uint32_t ALERT_COOLDOWN = 300000UL;  // 5 min

static WiFiClientSecure _tls;

// ─────────────────────────────────────────────────────────
static void _loadPrefs() {
    Preferences p;
    p.begin("airmon", true);
    _enabled      = p.getBool("tg_en",       false);
    _token        = p.getString("tg_token",  "");
    _chatId       = p.getString("tg_chatid", "");
    _thr_co2      = (uint16_t)p.getInt("thr_co2",      1500);
    _thr_aqi      = (uint8_t) p.getInt("thr_aqi",         3);
    _thr_temp_hi  = p.getFloat("thr_temp_hi",          30.0f);
    _thr_hum_hi   = p.getFloat("thr_hum_hi",           75.0f);
    p.end();
}

// ── Save thresholds (called from web_server_module) ───────
void saveThresholds(uint16_t co2, uint8_t aqi, float tHi, float hHi) {
    _thr_co2     = co2;
    _thr_aqi     = aqi;
    _thr_temp_hi = tHi;
    _thr_hum_hi  = hHi;
    Preferences p;
    p.begin("airmon", false);
    p.putInt  ("thr_co2",     co2);
    p.putInt  ("thr_aqi",     aqi);
    p.putFloat("thr_temp_hi", tHi);
    p.putFloat("thr_hum_hi",  hHi);
    p.end();
    DBGF("[TG] Thresholds saved CO2=%d AQI=%d T>%.1f H>%.1f\n",
         co2, aqi, tHi, hHi);
}

ThresholdConfig getThresholds() {
    return {_thr_co2, _thr_aqi, _thr_temp_hi, _thr_hum_hi};
}

// ─────────────────────────────────────────────────────────
static String _apiPost(const String& method, const String& body) {
    if (!WiFi.isConnected() || _token.isEmpty()) return "";
    _tls.setInsecure();
    if (!_tls.connect("api.telegram.org", 443)) {
        DBGLN("[TG] TLS connect failed");
        return "";
    }
    String url = "/bot" + _token + "/" + method;
    String req = "POST " + url + " HTTP/1.1\r\n"
                 "Host: api.telegram.org\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: " + body.length() + "\r\n"
                 "Connection: close\r\n\r\n" + body;
    _tls.print(req);

    String resp = "";
    bool   inHeaders = true;
    uint32_t t0 = millis();
    while ((_tls.connected() || _tls.available()) && (millis() - t0 < 8000)) {
        if (_tls.available()) {
            String line = _tls.readStringUntil('\n');
            if (inHeaders) { if (line == "\r") inHeaders = false; }
            else resp += line;
        }
    }
    _tls.stop();
    return resp;
}

// ─────────────────────────────────────────────────────────
bool sendMessage(const String& text) {
    if (!_enabled || _token.isEmpty() || _chatId.isEmpty()) return false;
    DynamicJsonDocument body(512);
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
    String msg = "<b>🌿 Air Monitor PRO</b>\n──────────────\n";
    msg += "🌡 Температура: <b>" + String(d.temp,1) + " °C</b>\n";
    msg += "💧 Влажность:   <b>" + String(d.hum,1)  + " %</b>\n";
    msg += "🫁 CO₂:         <b>" + String(d.co2)    + " ppm</b>\n";
    msg += "🧪 TVOC:        <b>" + String(d.tvoc)   + " ppb</b>\n";
    msg += String(aqiEmoji)+" AQI: <b>"+String(d.aqi)+"/5</b>\n";
    msg += "──────────────\n";
    msg += "📡 IP: " + s.ip + "\n";
    msg += "🕐 " + s.date_str + " " + s.time_str + "\n";
    msg += "⏱ Uptime: "+String(s.uptime/3600)+"h "+String((s.uptime%3600)/60)+"m\n";
    msg += "──────────────\n<i>Пороги: CO₂&gt;"+String(_thr_co2)+
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
    if (resp.isEmpty()) return;

    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return;
    if (!doc["ok"].as<bool>()) return;

    for (JsonObject upd : doc["result"].as<JsonArray>()) {
        int64_t uid = upd["update_id"].as<int64_t>();
        if (uid > _lastUpdateId) _lastUpdateId = uid;
        if (upd["message"]["chat"]["id"].as<String>() != _chatId) continue;

        String text = upd["message"]["text"].as<String>();
        DBGF("[TG] cmd: %s\n", text.c_str());

        if (text=="/status"||text=="/start") sendMessage(_buildStatus(d,s));
        else if (text=="/report") {
            DynamicJsonDocument jd(256);
            jd["temp"]=d.temp; jd["hum"]=d.hum; jd["co2"]=d.co2;
            jd["tvoc"]=d.tvoc; jd["aqi"]=d.aqi; jd["time"]=s.time_str;
            String jstr; serializeJson(jd,jstr);
            sendMessage("<pre>"+jstr+"</pre>");
        }
        else if (text=="/thresholds") {
            sendMessage("⚙️ <b>Текущие пороги алертов:</b>\n"
                        "CO₂ &gt; " + String(_thr_co2) + " ppm\n"
                        "AQI ≥ "    + String(_thr_aqi)  + "\n"
                        "Температура &gt; " + String(_thr_temp_hi,1) + " °C\n"
                        "Влажность &gt; "   + String(_thr_hum_hi,1)  + " %\n\n"
                        "<i>Изменить: Settings → Telegram → Thresholds</i>");
        }
        else if (text=="/reboot") {
            sendMessage("🔄 Перезагружаюсь..."); delay(500); ESP.restart();
        }
        else if (text=="/help") {
            sendMessage("<b>Команды:</b>\n"
                        "/status — показатели\n/report — JSON\n"
                        "/thresholds — пороги алертов\n"
                        "/reboot — перезагрузка\n/help — справка");
        }
    }
}

// ─────────────────────────────────────────────────────────
void begin() {
    _loadPrefs();
    if (!_enabled || _token.isEmpty() || _chatId.isEmpty()) {
        DBGLN("[TG] Disabled or not configured"); _enabled=false; return;
    }
    DBGF("[TG] Ready chat_id=%s CO2thr=%d AQIthr=%d\n",
         _chatId.c_str(), _thr_co2, _thr_aqi);
    sendMessage("🟢 <b>Air Monitor PRO</b> запущен!\nIP: " +
                WiFi.localIP().toString() +
                "\nПороги: CO₂&gt;"+String(_thr_co2)+" AQI≥"+String(_thr_aqi));
}

void loop(const SensorData& d, const SystemStatus& s) {
    if (!_enabled || !WiFi.isConnected()) return;
    uint32_t now = millis();
    if (now - _lastPoll < POLL_INTERVAL) return;
    _lastPoll = now;

    if (d.co2 > _thr_co2 && (now-_lastAlertCO2 > ALERT_COOLDOWN)) {
        _lastAlertCO2 = now;
        sendMessage("⚠️ <b>Высокий CO₂!</b>\nТекущий: <b>"+String(d.co2)+
                    " ppm</b>\nПорог: "+String(_thr_co2)+" ppm\n\nПроветрите!");
    }
    if (d.aqi >= _thr_aqi && (now-_lastAlertAQI > ALERT_COOLDOWN)) {
        _lastAlertAQI = now;
        const char* lvl = d.aqi==3?"умеренный":d.aqi==4?"плохой":"опасный";
        sendMessage("🔴 <b>Плохой воздух!</b>\nAQI: <b>"+String(d.aqi)+
                    "/5 ("+lvl+")</b>\nTVOC: "+String(d.tvoc)+" ppb");
    }
    if (d.temp > _thr_temp_hi && (now-_lastAlertTemp > ALERT_COOLDOWN)) {
        _lastAlertTemp = now;
        sendMessage("🌡 <b>Высокая температура!</b>\n<b>"+
                    String(d.temp,1)+" °C</b> (порог: "+
                    String(_thr_temp_hi,1)+" °C)");
    }
    if (d.hum > _thr_hum_hi && (now-_lastAlertHum > ALERT_COOLDOWN)) {
        _lastAlertHum = now;
        sendMessage("💧 <b>Высокая влажность!</b>\n<b>"+
                    String(d.hum,1)+" %</b> (порог: "+
                    String(_thr_hum_hi,1)+" %)");
    }

    _pollUpdates(d, s);
}

bool isEnabled() { return _enabled; }

} // namespace TelegramModule
