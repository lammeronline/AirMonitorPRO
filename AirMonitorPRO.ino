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
static uint32_t _lastSdUsageRead = 0;
static uint32_t _lastHealthLog   = 0;
static uint32_t _lastHistoryMs   = 0;
static size_t   _lastHistoryPts  = 0;
static uint32_t _sdUsedMb        = 0;
static uint32_t _sdTotalMb       = 0;
static uint8_t  _sdPct           = 0;

static const uint32_t HISTORY_SAVE_INTERVAL = 300000UL; // 5 min
static const uint32_t SD_USAGE_INTERVAL     = 30000UL;  // 30 s
static const uint32_t HEALTH_LOG_INTERVAL   = 60000UL;  // 60 s
static const size_t   HISTORY_HTTP_POINTS   = 360;

// ── /api/history JSON builder (called inside lambda) ──────
static String _buildHistoryJSON() {
    size_t total = g_history.size();
    size_t n = total;
    size_t step = 1;
    if (n > HISTORY_HTTP_POINTS) {
        step = (n + HISTORY_HTTP_POINTS - 1) / HISTORY_HTTP_POINTS;
        n = (n + step - 1) / step;
    }

    String j;
    j.reserve(n * 28 + 64);
    j = "{\"n\":" + String(n) + ",\"temp\":[";
    size_t outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(g_history.at(i).temp, 1);
    }
    j += "],\"hum\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(g_history.at(i).hum, 1);
    }
    j += "],\"co2\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(g_history.at(i).co2);
    }
    j += "],\"tvoc\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(g_history.at(i).tvoc);
    }
    j += "]}";
    _lastHistoryPts = n;
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
        uint32_t t0 = millis();
        uint32_t heapBefore = ESP.getFreeHeap();
        String json = _buildHistoryJSON();
        _lastHistoryMs = millis() - t0;
        DBGF("[Web] /api/history pts=%u bytes=%u heap=%u->%u took=%ums\n",
             (unsigned)_lastHistoryPts, (unsigned)json.length(),
             (unsigned)heapBefore, (unsigned)ESP.getFreeHeap(),
             (unsigned)_lastHistoryMs);
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
    WebServerModule::loop();

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
    if (_lastSdUsageRead == 0 || now - _lastSdUsageRead >= SD_USAGE_INTERVAL) {
        _lastSdUsageRead = now;
        SDLogger::getUsage(_sdUsedMb, _sdTotalMb, _sdPct);
    }
    g_status.sd_used_mb = _sdUsedMb;
    g_status.sd_total_mb = _sdTotalMb;
    g_status.sd_pct = _sdPct;
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

    if (_lastHealthLog == 0 || now - _lastHealthLog >= HEALTH_LOG_INTERVAL) {
        _lastHealthLog = now;
        DBGF("[Health] heap=%u minHeap=%u wsClients=%u histPts=%u histMs=%u wifi=%s tg=%s\n",
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned)WebServerModule::connectedClients(),
             (unsigned)g_history.size(),
             (unsigned)_lastHistoryMs,
             WifiManager::isConnected() ? "up" : "down",
             TelegramModule::isEnabled() ? "on" : "off");
    }

    yield();
}
