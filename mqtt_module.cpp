#include "mqtt_module.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>   // Nick O'Leary — install via Library Manager
#include <ArduinoJson.h>
#include <Preferences.h>

namespace MQTTModule {

// ---- Config loaded from NVS ----------------------------
static bool    _enabled   = false;
static String  _host;
static uint16_t _port     = 1883;
static String  _user;
static String  _pass;
static String  _topic     = "airmonitor";

// ---- Runtime -------------------------------------------
static WiFiClient    _wifiClient;
static PubSubClient  _client(_wifiClient);
static uint32_t      _lastReconnect  = 0;
static uint32_t      _lastPublish    = 0;
static const uint32_t RECONNECT_INTERVAL = 10000UL;
static const uint32_t PUBLISH_INTERVAL   = 30000UL;  // publish every 30s

// ---- Forward ----
static void _connect();
static void _onMessage(char* topic, byte* payload, unsigned int len);

// -------------------------------------------------------
static void _loadPrefs() {
    Preferences p;
    p.begin("airmon", true);
    _enabled = p.getBool("mqtt_en",    false);
    _host    = p.getString("mqtt_host","");
    _port    = (uint16_t)p.getInt("mqtt_port", 1883);
    _user    = p.getString("mqtt_user","");
    _pass    = p.getString("mqtt_pass","");
    _topic   = p.getString("mqtt_topic","airmonitor");
    p.end();
}

void begin() {
    _loadPrefs();
    if (!_enabled || _host.isEmpty()) {
        DBGLN("[MQTT] Disabled or no host configured");
        return;
    }
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

// -------------------------------------------------------
static void _connect() {
    if (!WiFi.isConnected()) return;

    String clientId = "AirMonitor-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    DBGF("[MQTT] Connecting to %s:%d as %s ...\n",
         _host.c_str(), _port, clientId.c_str());

    bool ok;
    if (_user.isEmpty()) {
        ok = _client.connect(clientId.c_str());
    } else {
        ok = _client.connect(clientId.c_str(), _user.c_str(), _pass.c_str());
    }

    if (ok) {
        DBGLN("[MQTT] Connected ✓");
        // Subscribe to command topic
        String cmdTopic = _topic + "/cmd";
        _client.subscribe(cmdTopic.c_str());
        // Announce online
        _client.publish((_topic + "/status").c_str(), "online", true);
    } else {
        DBGF("[MQTT] Failed, rc=%d\n", _client.state());
    }
}

// -------------------------------------------------------
static void _onMessage(char* topic, byte* payload, unsigned int len) {
    String msg;
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
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

// -------------------------------------------------------
void loop() {
    if (!_enabled || _host.isEmpty()) return;

    if (!_client.connected()) {
        uint32_t now = millis();
        if (now - _lastReconnect >= RECONNECT_INTERVAL) {
            _lastReconnect = now;
            _connect();
        }
        return;
    }
    _client.loop();
}

// -------------------------------------------------------
void publish(const SensorData& d, const SystemStatus& s) {
    if (!_enabled || !_client.connected()) return;
    uint32_t now = millis();
    if (now - _lastPublish < PUBLISH_INTERVAL) return;
    _lastPublish = now;

    // Individual topics
    _client.publish((_topic+"/temp").c_str(),  String(d.temp,  1).c_str());
    _client.publish((_topic+"/hum").c_str(),   String(d.hum,   1).c_str());
    _client.publish((_topic+"/co2").c_str(),   String(d.co2).c_str());
    _client.publish((_topic+"/tvoc").c_str(),  String(d.tvoc).c_str());
    _client.publish((_topic+"/aqi").c_str(),   String(d.aqi).c_str());
    _client.publish((_topic+"/rssi").c_str(),  String(s.rssi).c_str());

    // Full JSON payload on <topic>/json
    DynamicJsonDocument doc(256);
    doc["temp"]   = d.temp;
    doc["hum"]    = d.hum;
    doc["co2"]    = d.co2;
    doc["tvoc"]   = d.tvoc;
    doc["aqi"]    = d.aqi;
    doc["rssi"]   = s.rssi;
    doc["uptime"] = s.uptime;
    doc["ip"]     = s.ip;
    doc["time"]   = s.time_str;
    String json;
    serializeJson(doc, json);
    _client.publish((_topic+"/json").c_str(), json.c_str());

    DBGF("[MQTT] Published → %s/json\n", _topic.c_str());
}

bool isConnected() { return _enabled && _client.connected(); }
bool isEnabled()   { return _enabled; }

} // namespace MQTTModule
