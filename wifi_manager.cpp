#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>

namespace WifiManager {

static Preferences _prefs;
static Mode        _mode    = Mode::NONE;
static uint32_t    _retryTs = 0;
static uint8_t     _retries = 0;
static const uint8_t MAX_RETRIES = 5;

// -------------------------------------------------------
static void _startAP() {
    DBGLN("[WiFi] Starting AP: " AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : nullptr);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    _mode = Mode::AP;
    DBGF("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

static void _startSTA(const String& ssid, const String& pass) {
    DBGF("[WiFi] Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    _mode    = Mode::STA;
    _retries = 0;
    _retryTs = millis();
}

// -------------------------------------------------------
void begin() {
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

#if FORCE_FACTORY_RESET
    factoryReset();
#endif

    _prefs.begin("airmon", false);
    String ssid = _prefs.getString("ssid", "");
    String pass = _prefs.getString("pass", "");
    _prefs.end();

    if (ssid.isEmpty()) {
        _startAP();
    } else {
        _startSTA(ssid, pass);
    }
}

void loop() {
    if (_mode == Mode::STA) {
        if (!isConnected()) {
            uint32_t now = millis();
            if (now - _retryTs > 10000UL) {
                _retryTs = now;
                _retries++;
                DBGF("[WiFi] Retry %d/%d\n", _retries, MAX_RETRIES);
                if (_retries >= MAX_RETRIES) {
                    DBGLN("[WiFi] Max retries — switching to AP");
                    _startAP();
                } else {
                    WiFi.reconnect();
                }
            }
        } else {
            _retries = 0;
        }
    }
}

Mode    currentMode()  { return _mode; }
bool    isConnected()  { return WiFi.status() == WL_CONNECTED; }
int8_t  getRSSI()      { return (int8_t)WiFi.RSSI(); }
String  getIP()        {
    if (_mode == Mode::AP) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

String scanNetworks() {
    int n = WiFi.scanNetworks(false, true);
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) +
                "\",\"rssi\":" + WiFi.RSSI(i) +
                ",\"enc\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "0" : "1") + "}";
    }
    json += "]";
    WiFi.scanDelete();
    return json;
}

void saveCredentials(const String& ssid, const String& pass) {
    DBGF("[WiFi] Saving creds SSID=%s\n", ssid.c_str());
    _prefs.begin("airmon", false);
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.end();
    delay(300);
    ESP.restart();
}

void factoryReset() {
    DBGLN("[WiFi] Factory reset — clearing NVS");
    _prefs.begin("airmon", false);
    _prefs.clear();
    _prefs.end();
}

} // namespace WifiManager
