#include "sd_logger.h"
#include "config.h"
#include "runtime_settings.h"
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>

namespace SDLogger {

static bool     _sdOK    = false;
static uint32_t _lastLog = 0;
static uint32_t _lastLogBucket = 0;

static const char CACHE_DIR[] = "/cache";
static const char HIST_FILE[] = "/cache/history.bin";

// ── Magic header so we don't load corrupt files ──────────
static const uint32_t HIST_MAGIC = 0xA1B2C3D4;

// ─────────────────────────────────────────────────────────
void begin() {
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
        _sdOK = true;
        DBGLN("[SD] Card OK");
        if (!SD.exists(LOG_DIR))   SD.mkdir(LOG_DIR);
        if (!SD.exists(CACHE_DIR)) SD.mkdir(CACHE_DIR);
    } else {
        DBGLN("[SD] Card FAIL — running without SD");
    }
}

// ─────────────────────────────────────────────────────────
void loop(const SensorData& d, const String& datetime, const String& dateOnly,
          uint32_t epochSec, bool timeValid) {
    if (!_sdOK || !timeValid || epochSec == 0) return;
    auto cfg = RuntimeSettings::get();
    uint32_t step = (uint32_t)cfg.csv_interval_sec;
    uint32_t bucket = epochSec - (epochSec % step);
    if (_lastLogBucket == bucket) return;
    _lastLogBucket = bucket;
    _lastLog = millis();

    String path = String(LOG_DIR) + "/" + dateOnly + ".csv";
    bool newFile = !SD.exists(path);
    File f = SD.open(path, FILE_APPEND);
    if (!f) { DBGLN("[SD] Open FAIL: " + path); return; }
    if (newFile) f.println("datetime,temp_c,hum_pct,co2_ppm,tvoc_ppb,aqi");
    f.printf("%s,%.1f,%.1f,%d,%d,%d\n",
             datetime.c_str(), d.temp, d.hum, d.co2, d.tvoc, d.aqi);
    f.close();
    DBGF("[SD] Logged → %s\n", path.c_str());
}

// ─────────────────────────────────────────────────────────
bool isOK() { return _sdOK; }

void getUsage(uint32_t& used, uint32_t& total, uint8_t& pct) {
    if (!_sdOK) { used=0; total=0; pct=0; return; }
    uint64_t t = SD.totalBytes();
    uint64_t u = SD.usedBytes();
    total = (uint32_t)(t / (1024*1024));
    used  = (uint32_t)(u / (1024*1024));
    pct   = total > 0 ? (uint8_t)(u * 100 / t) : 0;
}

// ─────────────────────────────────────────────────────────
//  History persistence
// ─────────────────────────────────────────────────────────
void saveHistory(const RingBuffer<HistoryPoint, HISTORY_24H_CAP>& hist24,
                 const RingBuffer<HistoryPoint, HISTORY_7D_CAP>& hist7,
                 const RingBuffer<HistoryPoint, HISTORY_30D_CAP>& hist30) {
    if (!_sdOK) return;
    if (SD.exists(HIST_FILE)) SD.remove(HIST_FILE);
    File f = SD.open(HIST_FILE, FILE_WRITE);
    if (!f) { DBGLN("[SD] History save FAIL"); return; }

    uint32_t magic = HIST_MAGIC;
    f.write((uint8_t*)&magic, 4);
    uint16_t c24 = (uint16_t)hist24.size();
    uint16_t c7  = (uint16_t)hist7.size();
    uint16_t c30 = (uint16_t)hist30.size();
    f.write((uint8_t*)&c24, 2);
    f.write((uint8_t*)&c7, 2);
    f.write((uint8_t*)&c30, 2);
    for (size_t i = 0; i < hist24.size(); i++) {
        HistoryPoint hp = hist24.at(i);
        f.write((uint8_t*)&hp, sizeof(HistoryPoint));
    }
    for (size_t i = 0; i < hist7.size(); i++) {
        HistoryPoint hp = hist7.at(i);
        f.write((uint8_t*)&hp, sizeof(HistoryPoint));
    }
    for (size_t i = 0; i < hist30.size(); i++) {
        HistoryPoint hp = hist30.at(i);
        f.write((uint8_t*)&hp, sizeof(HistoryPoint));
    }
    f.close();
    DBGF("[SD] History saved: 24h=%u 7d=%u 30d=%u\n", c24, c7, c30);
}

bool loadHistory(RingBuffer<HistoryPoint, HISTORY_24H_CAP>& hist24,
                 RingBuffer<HistoryPoint, HISTORY_7D_CAP>& hist7,
                 RingBuffer<HistoryPoint, HISTORY_30D_CAP>& hist30) {
    if (!_sdOK || !SD.exists(HIST_FILE)) return false;
    File f = SD.open(HIST_FILE, FILE_READ);
    if (!f) return false;

    uint32_t magic = 0;
    f.read((uint8_t*)&magic, 4);
    if (magic != HIST_MAGIC) { f.close(); DBGLN("[SD] History: bad magic"); return false; }

    uint16_t c24 = 0, c7 = 0, c30 = 0;
    f.read((uint8_t*)&c24, 2);
    f.read((uint8_t*)&c7, 2);
    f.read((uint8_t*)&c30, 2);
    if (c24 > HISTORY_24H_CAP) c24 = HISTORY_24H_CAP;
    if (c7 > HISTORY_7D_CAP) c7 = HISTORY_7D_CAP;
    if (c30 > HISTORY_30D_CAP) c30 = HISTORY_30D_CAP;

    uint32_t loaded = 0;
    for (uint16_t i = 0; i < c24; i++) {
        HistoryPoint hp;
        if (f.read((uint8_t*)&hp, sizeof(HistoryPoint)) != sizeof(HistoryPoint)) break;
        hist24.push(hp);
        loaded++;
    }
    for (uint16_t i = 0; i < c7; i++) {
        HistoryPoint hp;
        if (f.read((uint8_t*)&hp, sizeof(HistoryPoint)) != sizeof(HistoryPoint)) break;
        hist7.push(hp);
        loaded++;
    }
    for (uint16_t i = 0; i < c30; i++) {
        HistoryPoint hp;
        if (f.read((uint8_t*)&hp, sizeof(HistoryPoint)) != sizeof(HistoryPoint)) break;
        hist30.push(hp);
        loaded++;
    }
    f.close();
    DBGF("[SD] History loaded: %d points\n", loaded);
    return loaded > 0;
}

// ─────────────────────────────────────────────────────────
//  CSV export — stream directly from SD to HTTP client
// ─────────────────────────────────────────────────────────
bool streamExport(const String& date, void* serverPtr) {
    if (!_sdOK) return false;
    String path = String(LOG_DIR) + "/" + date + ".csv";
    if (!SD.exists(path)) return false;

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    WebServer* srv = (WebServer*)serverPtr;
    srv->sendHeader("Content-Disposition",
                    "attachment; filename=\"airmonitor_" + date + ".csv\"");
    srv->sendHeader("Content-Length", String(f.size()));
    srv->setContentLength(f.size());
    srv->send(200, "text/csv", "");

    // Stream in 512-byte chunks to avoid RAM overflow
    uint8_t buf[512];
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        if (n > 0) srv->client().write(buf, n);
    }
    f.close();
    return true;
}

// ─────────────────────────────────────────────────────────
//  List available log dates as JSON array
// ─────────────────────────────────────────────────────────
String listLogDates() {
    if (!_sdOK) return "[]";
    File dir = SD.open(LOG_DIR);
    if (!dir) return "[]";

    String json = "[";
    bool first = true;
    File entry = dir.openNextFile();
    while (entry) {
        String name = entry.name();   // e.g. "2026-04-10.csv"
        if (!entry.isDirectory() && name.endsWith(".csv")) {
            String date = name.substring(0, name.length() - 4);
            // strip leading path if present
            int sl = date.lastIndexOf('/');
            if (sl >= 0) date = date.substring(sl + 1);
            if (!first) json += ",";
            json += "\"" + date + "\"";
            first = false;
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return json + "]";
}

} // namespace SDLogger
