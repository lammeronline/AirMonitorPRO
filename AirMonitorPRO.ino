// ============================================================
//  Air Monitor PRO — Main Sketch  v1.1.0
//  ESP32 WROOM-32
// ============================================================

#include "config.h"
#include "data_types.h"
#include "wifi_manager.h"
#include "web_server_module.h"
#include "sensors.h"
#include "oled_display.h"
#include "sd_logger.h"
#include "rtc_module.h"
#include "mqtt_module.h"
#include "telegram_module.h"

// ── Global state ──────────────────────────────────────────
static SensorData   g_data;
static SystemStatus g_status;

// RAM history ring buffer — 1440 points = 24 h @ 1 min
RingBuffer<SensorData, DATA_BUFFER_SIZE> g_history;

// ── Timers ────────────────────────────────────────────────
static uint32_t _lastWsBroadcast = 0;
static uint32_t _lastHistorySave = 0;
static uint32_t _lastHistPush    = 0;
static uint32_t _startMs         = 0;

static const uint32_t HISTORY_SAVE_INTERVAL = 300000UL; // 5 min

// ── /api/history JSON builder (called inside lambda) ──────
static String _buildHistoryJSON() {
    size_t n = g_history.size();
    // Pre-allocate: each point ~30 chars * 4 arrays * 1440 ≈ 175 KB worst case
    // In practice with 1440 pts we get ~120 KB — fits in ESP32 heap (300 KB free)
    String j;
    j.reserve(n * 32);
    j = "{\"n\":" + String(n) + ",\"temp\":[";
    for (size_t i=0;i<n;i++){if(i)j+=",";j+=String(g_history.at(i).temp,1);}
    j += "],\"hum\":[";
    for (size_t i=0;i<n;i++){if(i)j+=",";j+=String(g_history.at(i).hum,1);}
    j += "],\"co2\":[";
    for (size_t i=0;i<n;i++){if(i)j+=",";j+=String(g_history.at(i).co2);}
    j += "],\"tvoc\":[";
    for (size_t i=0;i<n;i++){if(i)j+=",";j+=String(g_history.at(i).tvoc);}
    j += "]}";
    return j;
}

// ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    DBGLN("\n");
    DBGLN("╔══════════════════════════════╗");
    DBGLN("║   Air Monitor PRO  v" FW_VERSION "   ║");
    DBGLN("╚══════════════════════════════╝");
    _startMs = millis();

    SDLogger::begin();
    Sensors::begin();
    OLEDDisplay::begin();
    RTCModule::begin();
    WifiManager::begin();
    WebServerModule::begin();
    MQTTModule::begin();

    // Restore history from SD cache into RAM buffer
    SDLogger::loadHistory(g_history);
    DBGF("[Main] History: %d points in RAM\n", (int)g_history.size());

    // /api/history — serve RAM buffer as JSON
    WebServerModule::registerRoute("/api/history", [](){
        String json = _buildHistoryJSON();
        // Access server via module's internal send helper
        // WebServerModule exposes sendJSON() for this purpose
        WebServerModule::sendJSON(json);
    });

    DBGLN("[Main] Setup complete");
}

// ─────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    WifiManager::loop();

    static bool _tgStarted = false;
    if (WifiManager::isConnected()) {
        RTCModule::loop();
        MQTTModule::loop();
        if (!_tgStarted) { _tgStarted = true; TelegramModule::begin(); }
        TelegramModule::loop(g_data, g_status);
    }

    Sensors::loop();
    g_data = Sensors::latest();

    g_status.sd_ok      = SDLogger::isOK();
    SDLogger::getUsage(g_status.sd_used_mb, g_status.sd_total_mb, g_status.sd_pct);
    g_status.rtc_ok     = RTCModule::isOK();
    g_status.ens_ok     = Sensors::ensOK();
    g_status.aht_ok     = Sensors::ahtOK();
    g_status.ens_warmup = Sensors::ensWarmingUp();
    g_status.rssi       = WifiManager::getRSSI();
    g_status.ip         = WifiManager::getIP();
    g_status.time_str   = RTCModule::getTimeString();
    g_status.date_str   = RTCModule::getDateString();
    g_status.uptime     = (now - _startMs) / 1000UL;

    SDLogger::loop(g_data,
                   RTCModule::getDateTimeString(),
                   RTCModule::getDateString());

    // Push to RAM history every 60 s, only when ENS is operating OK
    if (now - _lastHistPush >= 60000UL) {
        _lastHistPush = now;
        if (Sensors::ensStatus() == 0) {
            g_history.push(g_data);
        }
    }

    // Persist history to SD every 5 min
    if (now - _lastHistorySave >= HISTORY_SAVE_INTERVAL) {
        _lastHistorySave = now;
        SDLogger::saveHistory(g_history, RTCModule::getDateString());
    }

    OLEDDisplay::loop(g_data, g_status);

    if (now - _lastWsBroadcast >= WS_BROADCAST_INTERVAL) {
        _lastWsBroadcast = now;
        WebServerModule::broadcastData(g_data, g_status);
        MQTTModule::publish(g_data, g_status);
    }

    WebServerModule::loop();
    yield();
}
