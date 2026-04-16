#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>

// ============================================================
//  WiFi Manager
//  STA mode with multi-level watchdog:
//
//   Level 1 — software reconnect (WiFi.reconnect / WiFi.begin)
//             every RETRY_INTERVAL_MS, up to SOFT_RETRY_MAX times
//
//   Level 2 — hard stack reset: WiFi.mode(OFF) → delay → STA
//             after SOFT_RETRY_MAX soft retries, up to HARD_RETRY_MAX times
//
//   Level 3 — ESP.restart() if the interface stays down
//             for more than REBOOT_WATCHDOG_MS (default 5 min).
//             This cures the "WiFi stack dead" bug on ESP32.
//
//   Level 4 — If still no credentials after all retries: AP mode.
// ============================================================

namespace WifiManager {

static Preferences _prefs;
static Mode        _mode           = Mode::NONE;
static uint32_t    _retryTs        = 0;
static uint32_t    _connectedSince = 0;
static uint32_t    _disconnectedTs = 0;   // when we first lost connection
static uint8_t     _softRetries    = 0;
static uint8_t     _hardRetries    = 0;
static String      _ssid;
static String      _pass;
static String      _hostname;
static DNSServer   _dns;

// ── Timing constants ─────────────────────────────────────
static const uint8_t  SOFT_RETRY_MAX     =  4;      // soft reconnects before hard reset
static const uint8_t  HARD_RETRY_MAX     =  3;      // hard resets before giving up → AP
static const uint32_t RETRY_INTERVAL_MS  = 10000UL; // between retry attempts
static const uint32_t HARD_RESET_DELAY   =  1500UL; // WiFi stack settle after mode(OFF)
static const uint32_t REBOOT_WATCHDOG_MS = 300000UL;// 5 min disconnected → ESP.restart()

// ── Helpers ───────────────────────────────────────────────
static const char* _statusName(wl_status_t st) {
    switch (st) {
        case WL_IDLE_STATUS:     return "IDLE";
        case WL_NO_SSID_AVAIL:   return "NO_SSID";
        case WL_SCAN_COMPLETED:  return "SCAN_DONE";
        case WL_CONNECTED:       return "CONNECTED";
        case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "LOST";
        case WL_DISCONNECTED:    return "DISCONNECTED";
        default:                 return "UNKNOWN";
    }
}

// Level 1 — soft reconnect
static void _softReconnect(wl_status_t st) {
    _softRetries++;
    DBGF("[WiFi] Soft retry %d/%d (status=%s)\n",
         _softRetries, SOFT_RETRY_MAX, _statusName(st));

    if (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED) {
        WiFi.disconnect(false, false);
        delay(50);
        WiFi.begin(_ssid.c_str(), _pass.c_str());
    } else {
        WiFi.reconnect();
    }
}

// Level 2 — hard stack reset: cycle WiFi power, then reconnect
static void _hardReconnect() {
    _hardRetries++;
    _softRetries = 0;
    DBGF("[WiFi] Hard stack reset #%d/%d\n", _hardRetries, HARD_RETRY_MAX);

    WiFi.disconnect(true, false);     // disconnect, erase NVS wifi config = false
    WiFi.mode(WIFI_OFF);
    delay(HARD_RESET_DELAY);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname.c_str());
    WiFi.setAutoReconnect(false);     // we manage reconnects ourselves
    WiFi.begin(_ssid.c_str(), _pass.c_str());
}

// ── Start AP ─────────────────────────────────────────────
static void _startAP() {
    DBGLN("[WiFi] Starting AP: " AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : nullptr);
    IPAddress apIP(AP_IP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(AP_SUBNET));
    _mode = Mode::AP;
    DBGF("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    _dns.start(AP_DNS_PORT, "*", apIP);   // captive portal
    DBGLN("[WiFi] Captive portal DNS started");
}

// ── Start STA ─────────────────────────────────────────────
static void _startSTA(const String& ssid, const String& pass) {
    _ssid = ssid;
    _pass = pass;
    DBGF("[WiFi] Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);     // manual control
    WiFi.setHostname(_hostname.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    _mode          = Mode::STA;
    _softRetries   = 0;
    _hardRetries   = 0;
    _retryTs       = millis();
    _connectedSince  = 0;
    _disconnectedTs  = millis();       // start watchdog from connection attempt
}

// ── Public API ────────────────────────────────────────────
void begin() {
    WiFi.persistent(false);           // never write credentials to NVS from SDK

#if FORCE_FACTORY_RESET
    DBGLN("[WiFi] FORCE_FACTORY_RESET active — clearing NVS");
    {
        Preferences p;
        p.begin("airmon", false);
        p.clear();
        p.end();
        DBGLN("[WiFi] NVS cleared successfully");
    }
    DBGLN("[WiFi] Please set FORCE_FACTORY_RESET to 0 and reflash");
#endif

    _prefs.begin("airmon", false);
    String ssid = _prefs.getString("ssid", "");
    String pass = _prefs.getString("pass", "");
    _hostname    = _prefs.getString("dev_name", DEVICE_NAME);
    _prefs.end();

    _hostname.replace(" ", "-");

    if (ssid.isEmpty()) {
        _startAP();
    } else {
        _startSTA(ssid, pass);
    }
}

void loop() {
    if (_mode == Mode::AP) {
        _dns.processNextRequest();
        return;
    }

    wl_status_t st  = WiFi.status();
    uint32_t    now = millis();

    // ── Connected ─────────────────────────────────────────
    if (st == WL_CONNECTED) {
        if (_connectedSince == 0) {
            _connectedSince  = now;
            _disconnectedTs  = 0;
            _softRetries     = 0;
            _hardRetries     = 0;
            DBGF("[WiFi] Connected: %s  RSSI=%d\n",
                 WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
        }
        return;
    }

    // ── Just lost connection — start watchdog ─────────────
    if (_connectedSince != 0) {
        _connectedSince = 0;
        _disconnectedTs = now;
        _retryTs        = now;
        DBGF("[WiFi] Connection lost (status=%s)\n", _statusName(st));
    }

    // ── Level 3 watchdog: reboot if offline too long ──────
    if (_disconnectedTs != 0 &&
        (now - _disconnectedTs) >= REBOOT_WATCHDOG_MS) {
        DBGF("[WiFi] Offline for %lu s — rebooting ESP32\n",
             (unsigned long)(now - _disconnectedTs) / 1000UL);
        delay(200);
        ESP.restart();
    }

    // Wait for next retry slot
    if (st == WL_IDLE_STATUS) return;
    if (now - _retryTs < RETRY_INTERVAL_MS) return;
    _retryTs = now;

    // ── Level 2: hard reset after enough soft retries ─────
    if (_softRetries >= SOFT_RETRY_MAX) {
        if (_hardRetries >= HARD_RETRY_MAX) {
            DBGLN("[WiFi] All retries exhausted — switching to AP");
            _startAP();
            return;
        }
        _hardReconnect();
        return;
    }

    // ── Level 1: soft reconnect ───────────────────────────
    _softReconnect(st);
}

// ── Accessors ─────────────────────────────────────────────
Mode   currentMode()  { return _mode; }
bool   isConnected()  { return WiFi.status() == WL_CONNECTED; }
bool   isStableConnected(uint32_t minStableMs) {
    return isConnected() && _connectedSince != 0 &&
           (millis() - _connectedSince >= minStableMs);
}
int8_t  getRSSI() { return (int8_t)WiFi.RSSI(); }
String  getIP()   {
    if (_mode == Mode::AP) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

// ── WiFi scan ─────────────────────────────────────────────
String scanNetworks() {
    int n = WiFi.scanNetworks(false, true);
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) +
                "\",\"rssi\":" + WiFi.RSSI(i) +
                ",\"enc\":"    + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "0" : "1") + "}";
    }
    json += "]";
    WiFi.scanDelete();
    return json;
}

// ── Save credentials & reboot ─────────────────────────────
void saveCredentials(const String& ssid, const String& pass) {
    DBGF("[WiFi] Saving creds SSID=%s\n", ssid.c_str());
    _prefs.begin("airmon", false);
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.end();
    delay(300);
    ESP.restart();
}

// ── Factory reset ─────────────────────────────────────────
void factoryReset() {
    DBGLN("[WiFi] Factory reset — clearing NVS");
    _prefs.begin("airmon", false);
    _prefs.clear();
    _prefs.end();
}

} // namespace WifiManager
