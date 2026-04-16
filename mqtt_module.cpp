#include "mqtt_module.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>   // Nick O'Leary — install via Library Manager
#include <ArduinoJson.h>
#include <Preferences.h>

namespace MQTTModule {

// ── Config loaded from NVS ───────────────────────────────
static bool     _enabled = false;
static String   _host;
static uint16_t _port    = MQTT_DEFAULT_PORT;
static String   _user;
static String   _pass;
static String   _topic   = "airmonitor";

// ── Pre-built topic strings (rebuilt on connect) ─────────
// Avoids String concatenation inside publish() on every call.
static String _tTmp, _tHum, _tCo2, _tTvoc, _tAqi, _tRssi, _tJson, _tStatus, _tCmd;

static void _rebuildTopics() {
    _tTmp    = _topic + "/temp";
    _tHum    = _topic + "/hum";
    _tCo2    = _topic + "/co2";
    _tTvoc   = _topic + "/tvoc";
    _tAqi    = _topic + "/aqi";
    _tRssi   = _topic + "/rssi";
    _tJson   = _topic + "/json";
    _tStatus = _topic + "/status";
    _tCmd    = _topic + "/cmd";
}

// ── Runtime ──────────────────────────────────────────────
static WiFiClient    _wifiClient;
static PubSubClient  _client(_wifiClient);
static uint32_t      _lastReconnect = 0;
static uint32_t      _lastPublish   = 0;
static const uint32_t RECONNECT_INTERVAL = 10000UL;
static const uint32_t PUBLISH_INTERVAL   = 30000UL;

// ── Forward ──────────────────────────────────────────────
static void _connect();
static void _onMessage(char* topic, byte* payload, unsigned int len);

// ── Prefs load ───────────────────────────────────────────
static void _loadPrefs() {
    Preferences p;
    p.begin("airmon", true);
    _enabled = p.getBool("mqtt_en",    false);
    _host    = p.getString("mqtt_host", "");
    _port    = (uint16_t)p.getInt("mqtt_port", MQTT_DEFAULT_PORT);
    _user    = p.getString("mqtt_user", "");
    _pass    = p.getString("mqtt_pass", "");
    _topic   = p.getString("mqtt_topic", "airmonitor");
    p.end();
}

void begin() {
    _loadPrefs();
    if (!_enabled || _host.isEmpty()) {
        DBGLN("[MQTT] Disabled or no host configured");
        return;
    }
    _rebuildTopics();
    _client.setServer(_host.c_str(), _port);
    _client.setKeepAlive(60);
    _client.setCallback(_onMessage);
    DBGF("[MQTT] Configured → %s:%d  topic=%s\n",
         _host.c_str(), _port, _topic.c_str());
}

void reload() {
    _client.disconnect();
    begin();
}

// ── Connect ──────────────────────────────────────────────
static void _connect() {
    if (!WiFi.isConnected()) return;

    // Use lower 4 bytes of MAC as unique client suffix
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "AirMonitor-%08X",
             (unsigned)ESP.getEfuseMac());

    DBGF("[MQTT] Connecting to %s:%d as %s ...\n",
         _host.c_str(), _port, clientId);

    const bool ok = _user.isEmpty()
        ? _client.connect(clientId)
        : _client.connect(clientId, _user.c_str(), _pass.c_str());

    if (ok) {
        DBGLN("[MQTT] Connected ✓");
        _client.subscribe(_tCmd.c_str());
        _client.publish(_tStatus.c_str(), "online", true);
    } else {
        DBGF("[MQTT] Failed, rc=%d\n", _client.state());
    }
}

// ── Incoming command handler ─────────────────────────────
static void _onMessage(char* topic, byte* payload, unsigned int len) {
    // Construct String from raw bytes — avoids char-by-char appending.
    const String msg((const char*)payload, len);
    DBGF("[MQTT] ← [%s] %s\n", topic, msg.c_str());

    if (msg == "reboot") {
        DBGLN("[MQTT] CMD: reboot");
        delay(200);
        ESP.restart();
    } else if (msg == "reset") {
        DBGLN("[MQTT] CMD: factory reset");
        Preferences p; p.begin("airmon", false); p.clear(); p.end();
        delay(200);
        ESP.restart();
    }
}

// ── Loop ─────────────────────────────────────────────────
void loop() {
    if (!_enabled || _host.isEmpty()) return;

    if (!_client.connected()) {
        const uint32_t now = millis();
        if (now - _lastReconnect >= RECONNECT_INTERVAL) {
            _lastReconnect = now;
            _connect();
        }
        return;
    }
    _client.loop();
}

// ── Publish ──────────────────────────────────────────────
void publish(const SensorData& d, const SystemStatus& s) {
    if (!_enabled || !_client.connected()) return;
    const uint32_t now = millis();
    if (now - _lastPublish < PUBLISH_INTERVAL) return;
    _lastPublish = now;

    // Numeric fields — use stack char[] to avoid heap String alloc per field.
    char buf[16];

    snprintf(buf, sizeof(buf), "%.1f",  d.temp); _client.publish(_tTmp.c_str(),  buf);
    snprintf(buf, sizeof(buf), "%.1f",  d.hum);  _client.publish(_tHum.c_str(),  buf);
    snprintf(buf, sizeof(buf), "%u",    d.co2);   _client.publish(_tCo2.c_str(),  buf);
    snprintf(buf, sizeof(buf), "%u",    d.tvoc);  _client.publish(_tTvoc.c_str(), buf);
    snprintf(buf, sizeof(buf), "%u",    d.aqi);   _client.publish(_tAqi.c_str(),  buf);
    snprintf(buf, sizeof(buf), "%d",    s.rssi);  _client.publish(_tRssi.c_str(), buf);

    // Full JSON payload on <topic>/json
    StaticJsonDocument<256> doc;
    doc["temp"]   = d.temp;
    doc["hum"]    = d.hum;
    doc["co2"]    = d.co2;
    doc["tvoc"]   = d.tvoc;
    doc["aqi"]    = d.aqi;
    doc["rssi"]   = s.rssi;
    doc["uptime"] = s.uptime;
    doc["ip"]     = s.ip;
    doc["time"]   = s.time_str;
    char json[256];
    serializeJson(doc, json, sizeof(json));
    _client.publish(_tJson.c_str(), json);

    DBGF("[MQTT] Published → %s/json\n", _topic.c_str());
}

bool isConnected() { return _enabled && _client.connected(); }
bool isEnabled()   { return _enabled; }

} // namespace MQTTModule
