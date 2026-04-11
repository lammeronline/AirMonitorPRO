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
#include "runtime_settings.h"
#include <time.h>

// ── Global state ──────────────────────────────────────────
static SensorData   g_data;
static SystemStatus g_status;

static RingBuffer<HistoryPoint, HISTORY_24H_CAP> g_hist24;
static RingBuffer<HistoryPoint, HISTORY_7D_CAP>  g_hist7;
static RingBuffer<HistoryPoint, HISTORY_30D_CAP> g_hist30;

struct HistoryAccumulator {
    bool     active = false;
    uint32_t bucket_start = 0;
    uint16_t count = 0;
    float    temp_sum = 0.0f;
    float    hum_sum = 0.0f;
    uint32_t co2_sum = 0;
    uint32_t tvoc_sum = 0;
};

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
static RuntimeSettings::HistoryConfig _activeHistCfg = {
    CSV_LOG_INTERVAL_DEFAULT_SEC,
    HIST_24H_INTERVAL_DEFAULT_MIN,
    HIST_7D_INTERVAL_DEFAULT_MIN,
    HIST_30D_INTERVAL_DEFAULT_MIN
};

static const uint32_t HISTORY_SAVE_INTERVAL = 300000UL; // 5 min
static const uint32_t SD_USAGE_INTERVAL     = 30000UL;  // 30 s
static const uint32_t HEALTH_LOG_INTERVAL   = 60000UL;  // 60 s
static const size_t   HISTORY_HTTP_POINTS   = 360;

static void _startBucket(HistoryAccumulator& acc, uint32_t bucketStart, const SensorData& d) {
    acc.active = true;
    acc.bucket_start = bucketStart;
    acc.count = 1;
    acc.temp_sum = d.temp;
    acc.hum_sum = d.hum;
    acc.co2_sum = d.co2;
    acc.tvoc_sum = d.tvoc;
}

static void _flushBucket24(HistoryAccumulator& acc) {
    if (!acc.active || acc.count == 0) return;
    HistoryPoint p;
    p.ts   = acc.bucket_start;
    p.temp = acc.temp_sum / acc.count;
    p.hum  = acc.hum_sum / acc.count;
    p.co2  = (uint16_t)(acc.co2_sum / acc.count);
    p.tvoc = (uint16_t)(acc.tvoc_sum / acc.count);
    g_hist24.push(p);
}

static void _flushBucket7(HistoryAccumulator& acc) {
    if (!acc.active || acc.count == 0) return;
    HistoryPoint p;
    p.ts   = acc.bucket_start;
    p.temp = acc.temp_sum / acc.count;
    p.hum  = acc.hum_sum / acc.count;
    p.co2  = (uint16_t)(acc.co2_sum / acc.count);
    p.tvoc = (uint16_t)(acc.tvoc_sum / acc.count);
    g_hist7.push(p);
}

static void _flushBucket30(HistoryAccumulator& acc) {
    if (!acc.active || acc.count == 0) return;
    HistoryPoint p;
    p.ts   = acc.bucket_start;
    p.temp = acc.temp_sum / acc.count;
    p.hum  = acc.hum_sum / acc.count;
    p.co2  = (uint16_t)(acc.co2_sum / acc.count);
    p.tvoc = (uint16_t)(acc.tvoc_sum / acc.count);
    g_hist30.push(p);
}

static bool _updateHistory24(HistoryAccumulator& acc,
                             uint32_t bucketSec,
                             uint32_t epochSec,
                             const SensorData& d) {
    if (bucketSec == 0 || epochSec == 0) return false;
    uint32_t bucketStart = epochSec - (epochSec % bucketSec);
    if (!acc.active) {
        _startBucket(acc, bucketStart, d);
        return false;
    }
    if (acc.bucket_start != bucketStart) {
        _flushBucket24(acc);
        _startBucket(acc, bucketStart, d);
        return true;
    }
    acc.count++;
    acc.temp_sum += d.temp;
    acc.hum_sum  += d.hum;
    acc.co2_sum  += d.co2;
    acc.tvoc_sum += d.tvoc;
    return false;
}

static bool _updateHistory7(HistoryAccumulator& acc,
                            uint32_t bucketSec,
                            uint32_t epochSec,
                            const SensorData& d) {
    if (bucketSec == 0 || epochSec == 0) return false;
    uint32_t bucketStart = epochSec - (epochSec % bucketSec);
    if (!acc.active) {
        _startBucket(acc, bucketStart, d);
        return false;
    }
    if (acc.bucket_start != bucketStart) {
        _flushBucket7(acc);
        _startBucket(acc, bucketStart, d);
        return true;
    }
    acc.count++;
    acc.temp_sum += d.temp;
    acc.hum_sum  += d.hum;
    acc.co2_sum  += d.co2;
    acc.tvoc_sum += d.tvoc;
    return false;
}

static bool _updateHistory30(HistoryAccumulator& acc,
                             uint32_t bucketSec,
                             uint32_t epochSec,
                             const SensorData& d) {
    if (bucketSec == 0 || epochSec == 0) return false;
    uint32_t bucketStart = epochSec - (epochSec % bucketSec);
    if (!acc.active) {
        _startBucket(acc, bucketStart, d);
        return false;
    }
    if (acc.bucket_start != bucketStart) {
        _flushBucket30(acc);
        _startBucket(acc, bucketStart, d);
        return true;
    }
    acc.count++;
    acc.temp_sum += d.temp;
    acc.hum_sum  += d.hum;
    acc.co2_sum  += d.co2;
    acc.tvoc_sum += d.tvoc;
    return false;
}

static String _formatHistoryLabel(uint32_t ts, const String& range) {
    time_t raw = (time_t)ts;
    struct tm t;
    localtime_r(&raw, &t);
    char buf[24];
    if (range == "24h") snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else if (range == "7d") snprintf(buf, sizeof(buf), "%02d.%02d %02d:00", t.tm_mday, t.tm_mon + 1, t.tm_hour);
    else snprintf(buf, sizeof(buf), "%02d.%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
    return String(buf);
}

static String _buildHistoryJSON24(const String& range) {
    const RingBuffer<HistoryPoint, HISTORY_24H_CAP>& buf = g_hist24;
    size_t total = buf.size();
    size_t n = total;
    size_t step = 1;
    if (n > HISTORY_HTTP_POINTS) {
        step = (n + HISTORY_HTTP_POINTS - 1) / HISTORY_HTTP_POINTS;
        n = (n + step - 1) / step;
    }

    String j;
    j.reserve(n * 60 + 128);
    j = "{\"range\":\"" + range + "\",\"n\":" + String(n) + ",\"labels\":[";
    size_t outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += "\"" + _formatHistoryLabel(buf.at(i).ts, range) + "\"";
    }
    j += "],\"temp\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).temp, 1);
    }
    j += "],\"hum\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).hum, 1);
    }
    j += "],\"co2\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).co2);
    }
    j += "],\"tvoc\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).tvoc);
    }
    j += "]}";
    _lastHistoryPts = n;
    return j;
}

static String _buildHistoryJSON7(const String& range) {
    const RingBuffer<HistoryPoint, HISTORY_7D_CAP>& buf = g_hist7;
    size_t total = buf.size();
    size_t n = total;
    size_t step = 1;
    if (n > HISTORY_HTTP_POINTS) {
        step = (n + HISTORY_HTTP_POINTS - 1) / HISTORY_HTTP_POINTS;
        n = (n + step - 1) / step;
    }

    String j;
    j.reserve(n * 60 + 128);
    j = "{\"range\":\"" + range + "\",\"n\":" + String(n) + ",\"labels\":[";
    size_t outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += "\"" + _formatHistoryLabel(buf.at(i).ts, range) + "\"";
    }
    j += "],\"temp\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).temp, 1);
    }
    j += "],\"hum\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).hum, 1);
    }
    j += "],\"co2\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).co2);
    }
    j += "],\"tvoc\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).tvoc);
    }
    j += "]}";
    _lastHistoryPts = n;
    return j;
}

static String _buildHistoryJSON30(const String& range) {
    const RingBuffer<HistoryPoint, HISTORY_30D_CAP>& buf = g_hist30;
    size_t total = buf.size();
    size_t n = total;
    size_t step = 1;
    if (n > HISTORY_HTTP_POINTS) {
        step = (n + HISTORY_HTTP_POINTS - 1) / HISTORY_HTTP_POINTS;
        n = (n + step - 1) / step;
    }

    String j;
    j.reserve(n * 60 + 128);
    j = "{\"range\":\"" + range + "\",\"n\":" + String(n) + ",\"labels\":[";
    size_t outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += "\"" + _formatHistoryLabel(buf.at(i).ts, range) + "\"";
    }
    j += "],\"temp\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).temp, 1);
    }
    j += "],\"hum\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).hum, 1);
    }
    j += "],\"co2\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).co2);
    }
    j += "],\"tvoc\":[";
    outIdx = 0;
    for (size_t i = 0; i < total; i += step) {
        if (outIdx++) j += ",";
        j += String(buf.at(i).tvoc);
    }
    j += "]}";
    _lastHistoryPts = n;
    return j;
}

static String _buildSelectedHistoryJSON(const String& range) {
    if (range == "7d") return _buildHistoryJSON7("7d");
    if (range == "30d") return _buildHistoryJSON30("30d");
    return _buildHistoryJSON24("24h");
}

static void _resetHistoryState(const RuntimeSettings::HistoryConfig& cfg) {
    g_hist24 = RingBuffer<HistoryPoint, HISTORY_24H_CAP>();
    g_hist7  = RingBuffer<HistoryPoint, HISTORY_7D_CAP>();
    g_hist30 = RingBuffer<HistoryPoint, HISTORY_30D_CAP>();
    _acc24 = HistoryAccumulator();
    _acc7  = HistoryAccumulator();
    _acc30 = HistoryAccumulator();
    _activeHistCfg = cfg;
    _hist24Rev++;
    _hist7Rev++;
    _hist30Rev++;
    DBGF("[Main] History intervals updated: csv=%us 24h=%umin 7d=%umin 30d=%umin\n",
         cfg.csv_interval_sec, cfg.hist24_interval_min,
         cfg.hist7_interval_min, cfg.hist30_interval_min);
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
    RuntimeSettings::begin();
    _activeHistCfg = RuntimeSettings::get();
    WifiManager::begin();
    WebServerModule::begin();
    MQTTModule::begin();

    SDLogger::loadHistory(g_hist24, g_hist7, g_hist30);
    DBGF("[Main] History: 24h=%d 7d=%d 30d=%d\n",
         (int)g_hist24.size(), (int)g_hist7.size(), (int)g_hist30.size());

    WebServerModule::registerRoute("/api/history", [](){
        String range = WebServerModule::arg("range");
        if (range != "7d" && range != "30d") range = "24h";
        uint32_t t0 = millis();
        uint32_t heapBefore = ESP.getFreeHeap();
        String json = _buildSelectedHistoryJSON(range);
        _lastHistoryMs = millis() - t0;
        DBGF("[Web] /api/history range=%s pts=%u bytes=%u heap=%u->%u took=%ums\n",
             range.c_str(), (unsigned)_lastHistoryPts, (unsigned)json.length(),
             (unsigned)heapBefore, (unsigned)ESP.getFreeHeap(),
             (unsigned)_lastHistoryMs);
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
    g_data.ts = RTCModule::getEpoch();
    bool timeValid = RTCModule::hasValidTime();

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
                   RTCModule::getDateString(),
                   g_data.ts,
                   timeValid);

    auto cfg = RuntimeSettings::get();
    if (cfg.hist24_interval_min != _activeHistCfg.hist24_interval_min ||
        cfg.hist7_interval_min != _activeHistCfg.hist7_interval_min ||
        cfg.hist30_interval_min != _activeHistCfg.hist30_interval_min) {
        _resetHistoryState(cfg);
    } else if (cfg.csv_interval_sec != _activeHistCfg.csv_interval_sec) {
        _activeHistCfg.csv_interval_sec = cfg.csv_interval_sec;
    }
    if (timeValid && Sensors::ensStatus() == 0) {
        if (_updateHistory24(_acc24, cfg.hist24_interval_min * 60UL, g_data.ts, g_data)) _hist24Rev++;
        if (_updateHistory7(_acc7,   cfg.hist7_interval_min * 60UL,  g_data.ts, g_data)) _hist7Rev++;
        if (_updateHistory30(_acc30, cfg.hist30_interval_min * 60UL, g_data.ts, g_data)) _hist30Rev++;
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
        DBGF("[Health] heap=%u minHeap=%u wsClients=%u hist24=%u hist7=%u hist30=%u histMs=%u wifi=%s tg=%s\n",
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned)WebServerModule::connectedClients(),
             (unsigned)g_hist24.size(),
             (unsigned)g_hist7.size(),
             (unsigned)g_hist30.size(),
             (unsigned)_lastHistoryMs,
             WifiManager::isConnected() ? "up" : "down",
             TelegramModule::isEnabled() ? "on" : "off");
    }

    yield();
}
