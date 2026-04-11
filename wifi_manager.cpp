#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>

namespace WifiManager {

static Preferences _prefs;
static Mode        _mode    = Mode::NONE;
static uint32_t    _retryTs = 0;
static uint32_t    _connectedSince = 0;
static uint8_t     _retries = 0;
static String      _ssid;
static String      _pass;
static const uint8_t MAX_RETRIES = 5;
static const uint32_t RETRY_INTERVAL_MS = 10000UL;

static const char* _statusName(wl_status_t st) {
    switch (st) {
        case WL_IDLE_STATUS:    return "IDLE";
        case WL_NO_SSID_AVAIL:  return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_DONE";
        case WL_CONNECTED:      return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST:return "CONNECTION_LOST";
        case WL_DISCONNECTED:   return "DISCONNECTED";
        default:                return "UNKNOWN";
    }
}

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
    _ssid = ssid;
    _pass = pass;
    DBGF("[WiFi] Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    _mode    = Mode::STA;
    _retries = 0;
    _retryTs = millis();
    _connectedSince = 0;
}

static void _retrySTA(wl_status_t st) {
    if (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED) {
        DBGF("[WiFi] Hard retry: %s\n", _statusName(st));
        WiFi.disconnect(false, false);
        delay(50);
        WiFi.begin(_ssid.c_str(), _pass.c_str());
        return;
    }

    DBGF("[WiFi] Reconnect: %s\n", _statusName(st));
    WiFi.reconnect();
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
    if (_mode != Mode::STA) return;

    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        if (_connectedSince == 0) {
            _connectedSince = millis();
            DBGF("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
        }
        _retries = 0;
        return;
    }

    _connectedSince = 0;

    uint32_t now = millis();
    if (st == WL_IDLE_STATUS) {
        if (now - _retryTs >= RETRY_INTERVAL_MS) {
            _retryTs = now;
            DBGLN("[WiFi] STA still connecting — waiting");
        }
        return;
    }

    if (now - _retryTs < RETRY_INTERVAL_MS) return;

    _retryTs = now;
    _retries++;
    DBGF("[WiFi] Retry %d/%d (status=%s)\n",
         _retries, MAX_RETRIES, _statusName(st));

    if (_retries >= MAX_RETRIES) {
        DBGLN("[WiFi] Max retries — switching to AP");
        _startAP();
        return;
    }

    _retrySTA(st);
}

Mode    currentMode()  { return _mode; }
bool    isConnected()  { return WiFi.status() == WL_CONNECTED; }
bool    isStableConnected(uint32_t minStableMs) {
    return isConnected() && _connectedSince != 0 &&
           (millis() - _connectedSince >= minStableMs);
}
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
