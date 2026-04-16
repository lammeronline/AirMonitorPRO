// ============================================================
//  Air Monitor PRO — Main Sketch  v1.2.0
//  ESP32 WROOM-32
// ============================================================

#include "config.h"
#include "data_types.h"
#include "history_engine.h"      // template helpers — must be before other includes
#include "wifi_manager.h"
#include "web_server_module.h"
#include "sensors.h"
#include "oled_display.h"
#include "sd_logger.h"
#include "rtc_module.h"
#include "mqtt_module.h"
#include "telegram_module.h"
#include "runtime_settings.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

// ── Runtime debug flag (web UI → System → Debug Mode) ────
bool g_debug_enabled = false;

// ── Global state ──────────────────────────────────────────
static SensorData   g_data;
static SystemStatus g_status;

static RingBuffer<HistoryPoint, HISTORY_24H_CAP> g_hist24;
static RingBuffer<HistoryPoint, HISTORY_7D_CAP>  g_hist7;
static RingBuffer<HistoryPoint, HISTORY_30D_CAP> g_hist30;

static HistoryAccumulator _acc24;
static HistoryAccumulator _acc7;
static HistoryAccumulator _acc30;

// ── Timers ────────────────────────────────────────────────
static uint32_t _lastWsBroadcast = 0;
static uint32_t _lastHistorySave = 0;
static uint32_t _startMs         = 0;
static uint32_t _lastSdUsageRead = 0;
static uint32_t _lastHealthLog   = 0;
static uint32_t _lastHistoryMs   = 0;
static size_t   _lastHistoryPts  = 0;
static uint32_t _sdUsedMb        = 0;
static uint32_t _sdTotalMb       = 0;
static uint8_t  _sdPct           = 0;
static uint32_t _hist24Rev       = 0;
static uint32_t _hist7Rev        = 0;
static uint32_t _hist30Rev       = 0;
static RuntimeSettings::HistoryConfig _activeCfg = {
    CSV_LOG_INTERVAL_DEFAULT_SEC,
    HIST_24H_INTERVAL_DEFAULT_MIN,
    HIST_7D_INTERVAL_DEFAULT_MIN,
    HIST_30D_INTERVAL_DEFAULT_MIN
};

static const uint32_t HISTORY_SAVE_INTERVAL = 300000UL;
static const uint32_t SD_USAGE_INTERVAL     =  30000UL;
static const uint32_t HEALTH_LOG_INTERVAL   =  60000UL;

// ─────────────────────────────────────────────────────────
static void _resetHistoryState(const RuntimeSettings::HistoryConfig& cfg) {
    g_hist24 = RingBuffer<HistoryPoint, HISTORY_24H_CAP>();
    g_hist7  = RingBuffer<HistoryPoint, HISTORY_7D_CAP>();
    g_hist30 = RingBuffer<HistoryPoint, HISTORY_30D_CAP>();
    _acc24 = HistoryAccumulator();
    _acc7  = HistoryAccumulator();
    _acc30 = HistoryAccumulator();
    _activeCfg = cfg;
    _hist24Rev++;
    _hist7Rev++;
    _hist30Rev++;
    DBGF("[Main] History intervals updated: csv=%us 24h=%umin 7d=%umin 30d=%umin\n",
         cfg.csv_interval_sec, cfg.hist24_interval_min,
         cfg.hist7_interval_min, cfg.hist30_interval_min);
}

// ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);

    // Load debug flag from NVS before first DBGLN
    {
        Preferences p;
        p.begin("airmon", true);
        g_debug_enabled = p.getBool("debug_en", false);
        p.end();
    }

    DBGLN("\n");
    DBGLN("╔══════════════════════════════╗");
    DBGLN("║   Air Monitor PRO  v" FW_VERSION "   ║");
    DBGLN("╚══════════════════════════════╝");
    _startMs = millis();

    SDLogger::begin();
    Sensors::begin();
    OLEDDisplay::begin();
    RTCModule::begin();
    RuntimeSettings::begin();
    _activeCfg = RuntimeSettings::get();
    WifiManager::begin();
    WebServerModule::begin();
    MQTTModule::begin();
    TelegramModule::init();  // Load Telegram settings from NVS

    SDLogger::loadHistory(g_hist24, g_hist7, g_hist30);
    DBGF("[Main] History: 24h=%d 7d=%d 30d=%d\n",
         (int)g_hist24.size(), (int)g_hist7.size(), (int)g_hist30.size());

    WebServerModule::registerRoute("/api/history", []() {
        const String range = WebServerModule::arg("range");
        const uint32_t t0  = millis();
        const uint32_t hb  = ESP.getFreeHeap();
        String json;
        if      (range == "7d")  json = histBuildJSON(g_hist7,  "7d",  _lastHistoryPts);
        else if (range == "30d") json = histBuildJSON(g_hist30, "30d", _lastHistoryPts);
        else                     json = histBuildJSON(g_hist24, "24h", _lastHistoryPts);
        _lastHistoryMs = millis() - t0;
        DBGF("[Web] /api/history range=%s pts=%u bytes=%u heap=%u->%u took=%ums\n",
             range.c_str(), (unsigned)_lastHistoryPts, (unsigned)json.length(),
             (unsigned)hb, (unsigned)ESP.getFreeHeap(), (unsigned)_lastHistoryMs);
        WebServerModule::sendJSON(json);
    });

    // REST fallback endpoint for live data (if WebSocket port 81 is blocked)
    WebServerModule::registerRoute("/api/data", [&]() {
        DynamicJsonDocument doc(768);
        doc["type"]       = "data";
        doc["temp"]       = g_data.temp;
        doc["hum"]        = g_data.hum;
        doc["co2"]        = g_data.co2;
        doc["tvoc"]       = g_data.tvoc;
        doc["aqi"]        = g_data.aqi;
        doc["rssi"]       = g_status.rssi;
        doc["sd"]         = g_status.sd_ok;
        doc["sd_used"]    = g_status.sd_used_mb;
        doc["sd_total"]   = g_status.sd_total_mb;
        doc["sd_pct"]     = g_status.sd_pct;
        doc["rtc"]        = g_status.rtc_ok;
        doc["ens"]        = g_status.ens_ok;
        doc["ens_status"] = Sensors::ensStatus();
        doc["aht"]        = g_status.aht_ok;
        doc["ip"]         = g_status.ip;
        doc["uptime"]     = g_status.uptime;
        doc["time_short"] = g_status.time_str;
        doc["date"]       = g_status.date_str;
        doc["ver"]        = FW_VERSION;
        doc["warmup"]     = g_status.ens_warmup;
        doc["warmup_pct"] = g_status.ens_warmup
            ? min(100.0f,(float)(millis()/(ENS160_WARMUP_MS/100.0f)))
            : 100.0f;
        doc["mqtt"]       = MQTTModule::isConnected();
        doc["tg"]         = TelegramModule::isEnabled();
        doc["hist24_rev"] = g_status.hist24_rev;
        doc["hist7_rev"]  = g_status.hist7_rev;
        doc["hist30_rev"] = g_status.hist30_rev;
        String out; serializeJson(doc,out);
        WebServerModule::sendJSON(out);
    });

    DBGLN("[Main] Setup complete");
}

// ─────────────────────────────────────────────────────────
void loop() {
    const uint32_t now = millis();

    WifiManager::loop();
    WebServerModule::loop();

    if (WifiManager::isConnected()) {
        RTCModule::loop();
        MQTTModule::loop();
        
        // Telegram: always check if it should be started (it may have been enabled via settings)
        // IMPORTANT: Save state to local variable to avoid race condition with other threads
        static bool _tgTaskStarted = false;
        bool tgEnabled = TelegramModule::isEnabled();  // Safe snapshot
        
        if (!_tgTaskStarted && tgEnabled) {
            _tgTaskStarted = true;
            DBGLN("[Main] Starting Telegram module");
            TelegramModule::begin();
        }
        // If disabled, reset the flag so it can start again if re-enabled
        if (_tgTaskStarted && !tgEnabled) {
            _tgTaskStarted = false;
            DBGLN("[Main] Telegram module disabled");
        }
        TelegramModule::loop(g_data, g_status);
    }

    Sensors::loop();
    g_data    = Sensors::latest();
    g_data.ts = RTCModule::getEpoch();
    const bool timeValid = RTCModule::hasValidTime();

    const String dateStr = RTCModule::getDateString();
    const String timeStr = RTCModule::getTimeString();

    g_status.sd_ok      = SDLogger::isOK();
    g_status.rtc_ok     = RTCModule::isOK();
    g_status.ens_ok     = Sensors::ensOK();
    g_status.aht_ok     = Sensors::ahtOK();
    g_status.ens_warmup = Sensors::ensWarmingUp();
    g_status.rssi       = WifiManager::getRSSI();
    g_status.ip         = WifiManager::getIP();
    g_status.time_str   = timeStr;
    g_status.date_str   = dateStr;
    g_status.uptime     = (now - _startMs) / 1000UL;

    if (_lastSdUsageRead == 0 || now - _lastSdUsageRead >= SD_USAGE_INTERVAL) {
        _lastSdUsageRead = now;
        SDLogger::getUsage(_sdUsedMb, _sdTotalMb, _sdPct);
    }
    g_status.sd_used_mb  = _sdUsedMb;
    g_status.sd_total_mb = _sdTotalMb;
    g_status.sd_pct      = _sdPct;

    SDLogger::loop(g_data, dateStr + " " + timeStr, dateStr, g_data.ts, timeValid);

    const auto cfg = RuntimeSettings::get();
    if (cfg.hist24_interval_min != _activeCfg.hist24_interval_min ||
        cfg.hist7_interval_min  != _activeCfg.hist7_interval_min  ||
        cfg.hist30_interval_min != _activeCfg.hist30_interval_min) {
        _resetHistoryState(cfg);
    } else if (cfg.csv_interval_sec != _activeCfg.csv_interval_sec) {
        _activeCfg.csv_interval_sec = cfg.csv_interval_sec;
    }

    if (timeValid && Sensors::ensStatus() == 0) {
        if (histUpdateBucket(_acc24, cfg.hist24_interval_min * 60UL, g_data.ts, g_data, g_hist24)) _hist24Rev++;
        if (histUpdateBucket(_acc7,  cfg.hist7_interval_min  * 60UL, g_data.ts, g_data, g_hist7))  _hist7Rev++;
        if (histUpdateBucket(_acc30, cfg.hist30_interval_min * 60UL, g_data.ts, g_data, g_hist30)) _hist30Rev++;
    }
    g_status.hist24_rev = _hist24Rev;
    g_status.hist7_rev  = _hist7Rev;
    g_status.hist30_rev = _hist30Rev;

    if (now - _lastHistorySave >= HISTORY_SAVE_INTERVAL) {
        _lastHistorySave = now;
        SDLogger::saveHistory(g_hist24, g_hist7, g_hist30);
    }

    OLEDDisplay::loop(g_data, g_status);

    if (now - _lastWsBroadcast >= WS_BROADCAST_INTERVAL) {
        _lastWsBroadcast = now;
        WebServerModule::broadcastData(g_data, g_status);
        MQTTModule::publish(g_data, g_status);
    }

    if (_lastHealthLog == 0 || now - _lastHealthLog >= HEALTH_LOG_INTERVAL) {
        _lastHealthLog = now;
        DBGF("[Health] heap=%u minHeap=%u wsClients=%u "
             "hist24=%u hist7=%u hist30=%u histMs=%u wifi=%s tg=%s\n",
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned)WebServerModule::connectedClients(),
             (unsigned)g_hist24.size(),
             (unsigned)g_hist7.size(),
             (unsigned)g_hist30.size(),
             (unsigned)_lastHistoryMs,
             WifiManager::isConnected()  ? "up"  : "down",
             TelegramModule::isEnabled() ? "on"  : "off");
    }

    yield();
}
