#include "web_server_module.h"
#include "web_ui.h"
#include "config.h"
#include "sensors.h"
#include "wifi_manager.h"
#include "ota_module.h"
#include "mqtt_module.h"
#include "telegram_module.h"
#include "sd_logger.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

namespace WebServerModule {

static WebServer        _http(WEB_PORT);
static WebSocketsServer _ws(WS_PORT);
static Preferences      _prefs;

// ── AP Setup page ─────────────────────────────────────────
static const char AP_PAGE[] PROGMEM = R"AP(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AirMonitor Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0b0f1a;color:#e2e8f0;font-family:-apple-system,sans-serif;
  display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;padding:20px}
.card{background:#111827;border:1px solid #1e2d45;border-radius:14px;padding:28px;width:100%;max-width:360px}
h1{font-size:20px;font-weight:700;color:#38bdf8;margin-bottom:4px}
p{color:#64748b;font-size:12px;margin-bottom:20px}
label{display:block;font-size:10px;color:#64748b;text-transform:uppercase;letter-spacing:1px;margin-bottom:4px}
input{width:100%;background:#1a2236;border:1px solid #1e2d45;color:#e2e8f0;
  padding:10px 12px;border-radius:8px;font-size:13px;outline:none;margin-bottom:12px}
input:focus{border-color:#38bdf8}
.net-item{display:flex;justify-content:space-between;padding:9px 11px;
  background:#1a2236;border:1px solid #1e2d45;border-radius:7px;
  cursor:pointer;margin-bottom:5px;font-size:12px}
.net-item:hover{border-color:#38bdf8}
button{width:100%;background:#1d4ed8;color:#fff;border:none;
  padding:12px;border-radius:8px;font-size:14px;font-weight:700;cursor:pointer;margin-top:4px}
#st{margin-top:14px;font-size:12px;color:#fbbf24;text-align:center}
</style></head><body>
<div class="card">
  <h1>🌿 Air Monitor</h1><p>WiFi Setup</p>
  <div id="nets" style="margin-bottom:12px"><p style="color:#64748b;font-size:12px">Loading...</p></div>
  <label>SSID</label><input id="ssid" placeholder="Network name">
  <label>Password</label><input id="pass" type="password" placeholder="Password">
  <button onclick="save()">Connect & Save</button>
  <div id="st"></div>
</div>
<script>
fetch('/api/scan').then(r=>r.json()).then(nets=>{
  const d=document.getElementById('nets');
  d.innerHTML=nets.sort((a,b)=>b.rssi-a.rssi).map(n=>
    `<div class="net-item" onclick="document.getElementById('ssid').value='${n.ssid}'">
      <span>${n.ssid}</span><span style="color:#64748b">${n.rssi}dBm${n.enc?' 🔒':''}</span>
    </div>`).join('');
});
function save(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value;
  if(!ssid){alert('Enter SSID');return;}
  document.getElementById('st').textContent='Saving...';
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid,pass})}).then(()=>{
    document.getElementById('st').textContent='✅ Saved — rebooting...';});
}
</script></body></html>
)AP";

// ── WebSocket events ──────────────────────────────────────
static void _wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (type != WStype_TEXT) return;
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "get_settings") == 0) {
        _prefs.begin("airmon", true);
        DynamicJsonDocument resp(768);
        resp["type"]        = "settings";
        resp["ssid"]        = _prefs.getString("ssid","");
        resp["mqtt_en"]     = _prefs.getBool("mqtt_en",false);
        resp["mqtt_host"]   = _prefs.getString("mqtt_host","");
        resp["mqtt_port"]   = _prefs.getInt("mqtt_port",1883);
        resp["mqtt_user"]   = _prefs.getString("mqtt_user","");
        resp["mqtt_topic"]  = _prefs.getString("mqtt_topic","airmonitor");
        resp["tg_en"]       = _prefs.getBool("tg_en",false);
        resp["tg_chatid"]   = _prefs.getString("tg_chatid","");
        // Thresholds
        resp["thr_co2"]     = _prefs.getInt("thr_co2",     1500);
        resp["thr_aqi"]     = _prefs.getInt("thr_aqi",        3);
        resp["thr_temp_hi"] = _prefs.getFloat("thr_temp_hi",30.0f);
        resp["thr_hum_hi"]  = _prefs.getFloat("thr_hum_hi", 75.0f);
        _prefs.end();
        String out; serializeJson(resp, out);
        _ws.sendTXT(num, out);
    }
}

// ── CORS helper ───────────────────────────────────────────
static void _cors() { _http.sendHeader("Access-Control-Allow-Origin","*"); }

// ── Route registration ────────────────────────────────────
static void _registerRoutes() {

    // Main page
    _http.on("/", HTTP_GET, [](){
        _cors();
        if (WifiManager::currentMode() == WifiManager::Mode::AP)
            _http.send_P(200,"text/html",AP_PAGE);
        else
            _http.send_P(200,"text/html",WEB_HTML);
    });

    // WiFi scan
    _http.on("/api/scan", HTTP_GET, [](){
        _cors();
        _http.send(200,"application/json",WifiManager::scanNetworks());
    });

    // Save WiFi credentials
    _http.on("/api/wifi", HTTP_POST, [](){
        _cors();
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc,_http.arg("plain"))!=DeserializationError::Ok){
            _http.send(400,"text/plain","Bad JSON"); return;
        }
        String ssid=doc["ssid"].as<String>();
        String pass=doc["pass"].as<String>();
        _http.send(200,"text/plain","OK");
        delay(100);
        WifiManager::saveCredentials(ssid,pass);
    });

    // Save generic settings (MQTT, Telegram, debug)
    _http.on("/api/settings", HTTP_POST, [](){
        _cors();
        DynamicJsonDocument doc(512);
        if (deserializeJson(doc,_http.arg("plain"))!=DeserializationError::Ok){
            _http.send(400,"text/plain","Bad JSON"); return;
        }
        _prefs.begin("airmon",false);
        if(doc.containsKey("mqtt_en"))    _prefs.putBool("mqtt_en",   doc["mqtt_en"]);
        if(doc.containsKey("mqtt_host"))  _prefs.putString("mqtt_host",doc["mqtt_host"].as<String>());
        if(doc.containsKey("mqtt_port"))  _prefs.putInt("mqtt_port",  doc["mqtt_port"]);
        if(doc.containsKey("mqtt_user"))  _prefs.putString("mqtt_user",doc["mqtt_user"].as<String>());
        if(doc.containsKey("mqtt_pass"))  _prefs.putString("mqtt_pass",doc["mqtt_pass"].as<String>());
        if(doc.containsKey("mqtt_topic")) _prefs.putString("mqtt_topic",doc["mqtt_topic"].as<String>());
        if(doc.containsKey("tg_en"))      _prefs.putBool("tg_en",     doc["tg_en"]);
        if(doc.containsKey("tg_token"))   _prefs.putString("tg_token", doc["tg_token"].as<String>());
        if(doc.containsKey("tg_chatid"))  _prefs.putString("tg_chatid",doc["tg_chatid"].as<String>());
        _prefs.end();
        MQTTModule::reload();
        _http.send(200,"text/plain","OK");
    });

    // Save alert thresholds
    _http.on("/api/thresholds", HTTP_POST, [](){
        _cors();
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc,_http.arg("plain"))!=DeserializationError::Ok){
            _http.send(400,"text/plain","Bad JSON"); return;
        }
        uint16_t co2 = doc["co2"]  | 1500;
        uint8_t  aqi = doc["aqi"]  | 3;
        float   tHi  = doc["temp"] | 30.0f;
        float   hHi  = doc["hum"]  | 75.0f;
        TelegramModule::saveThresholds(co2, aqi, tHi, hHi);
        _http.send(200,"text/plain","OK");
    });

    // Get thresholds (for UI on load)
    _http.on("/api/thresholds", HTTP_GET, [](){
        _cors();
        auto t = TelegramModule::getThresholds();
        DynamicJsonDocument doc(128);
        doc["co2"]  = t.co2_ppm;
        doc["aqi"]  = t.aqi_level;
        doc["temp"] = t.temp_hi;
        doc["hum"]  = t.hum_hi;
        String out; serializeJson(doc,out);
        _http.send(200,"application/json",out);
    });

    // CSV export: /api/export?date=2026-04-10
    _http.on("/api/export", HTTP_GET, [](){
        _cors();
        String date = _http.arg("date");
        if (date.isEmpty()) {
            // Return list of available dates
            _http.send(200,"application/json",SDLogger::listLogDates());
            return;
        }
        // Stream the file
        if (!SDLogger::streamExport(date, &_http)) {
            _http.send(404,"text/plain","No log for date: "+date);
        }
    });

    // History endpoint — returns JSON array for chart pre-load
    // Served by AirMonitorPRO.ino via the global ring buffer
    // (registered externally after begin())

    // ENS160 calibration trigger
    _http.on("/api/calibrate", HTTP_POST, [](){
        _cors();
        bool ok = Sensors::calibrateBaseline();
        DynamicJsonDocument resp(128);
        resp["ok"]  = ok;
        resp["msg"] = Sensors::calibrateStatusMsg();
        String out; serializeJson(resp,out);
        _http.send(200,"application/json",out);
    });

    // Telegram test
    _http.on("/api/tg_test", HTTP_GET, [](){
        _cors();
        bool ok = TelegramModule::sendMessage(
            "✅ <b>Тест работает!</b>\nAir Monitor PRO на связи.");
        _http.send(200,"text/plain",ok?"OK":"FAIL — check token/chat_id");
    });

    // Reboot
    _http.on("/api/reboot", HTTP_POST, [](){
        _cors(); _http.send(200,"text/plain","Rebooting...");
        delay(300); ESP.restart();
    });

    // Factory reset
    _http.on("/api/reset", HTTP_POST, [](){
        _cors();
        WifiManager::factoryReset();
        _http.send(200,"text/plain","Reset done");
        delay(300); ESP.restart();
    });

    _http.onNotFound([](){ _http.send(404,"text/plain","Not found"); });

    OTAModule::registerRoutes(_http);
}

// ─────────────────────────────────────────────────────────
void begin() {
    _ws.begin();
    _ws.onEvent(_wsEvent);
    _registerRoutes();
    _http.begin();
    DBGLN("[Web] HTTP:80 WS:81");
}

void loop() {
    _http.handleClient();
    _ws.loop();
}

// ── Register external route (used by main for /api/history) ──
void registerRoute(const String& path,
                   WebServer::THandlerFunction fn) {
    _http.on(path.c_str(), HTTP_GET, fn);
}

void broadcastData(const SensorData& d, const SystemStatus& s) {
    if (_ws.connectedClients() == 0) return;
    DynamicJsonDocument doc(768);
    doc["type"]       = "data";
    doc["temp"]       = d.temp;
    doc["hum"]        = d.hum;
    doc["co2"]        = d.co2;
    doc["tvoc"]       = d.tvoc;
    doc["aqi"]        = d.aqi;
    doc["rssi"]       = s.rssi;
    doc["sd"]         = s.sd_ok;
    doc["sd_used"]    = s.sd_used_mb;
    doc["sd_total"]   = s.sd_total_mb;
    doc["sd_pct"]     = s.sd_pct;
    doc["rtc"]        = s.rtc_ok;
    doc["ens"]        = s.ens_ok;
    doc["ens_status"] = Sensors::ensStatus();
    doc["aht"]        = s.aht_ok;
    doc["ip"]         = s.ip;
    doc["uptime"]     = s.uptime;
    doc["time_short"] = s.time_str;
    doc["date"]       = s.date_str;
    doc["ver"]        = FW_VERSION;
    doc["warmup"]     = s.ens_warmup;
    doc["warmup_pct"] = s.ens_warmup
        ? min(100.0f,(float)(millis()/(ENS160_WARMUP_MS/100.0f)))
        : 100.0f;
    doc["mqtt"]       = MQTTModule::isConnected();
    doc["tg"]         = TelegramModule::isEnabled();
    String out; serializeJson(doc,out);
    _ws.broadcastTXT(out);
}

void sendJSON(const String& json) {
    _http.sendHeader("Access-Control-Allow-Origin","*");
    _http.send(200,"application/json",json);
}
} // namespace WebServerModule
