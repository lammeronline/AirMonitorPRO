#include "sd_logger.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>

namespace SDLogger {

static bool     _sdOK    = false;
static uint32_t _lastLog = 0;

static const char CACHE_DIR[] = "/cache";
static const char HIST_FILE[] = "/cache/history.bin";

// ── Magic header so we don't load corrupt files ──────────
static const uint32_t HIST_MAGIC = 0xA1B2C3D4;

// ── Packed struct saved per history point ────────────────
struct HistPoint {
    float    temp;
    float    hum;
    uint16_t co2;
    uint16_t tvoc;
    uint8_t  aqi;
    char     time[6];   // "HH:MM\0"
};

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
void loop(const SensorData& d, const String& datetime, const String& dateOnly) {
    if (!_sdOK) return;
    uint32_t now = millis();
    if (now - _lastLog < LOG_INTERVAL) return;
    _lastLog = now;

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
void saveHistory(const RingBuffer<SensorData, DATA_BUFFER_SIZE>& buf,
                 const String& dateOnly) {
    if (!_sdOK) return;

    // Remove old file and rewrite (SD doesn't support seek-write cleanly)
    if (SD.exists(HIST_FILE)) SD.remove(HIST_FILE);
    File f = SD.open(HIST_FILE, FILE_WRITE);
    if (!f) { DBGLN("[SD] History save FAIL"); return; }

    // Header: magic + date string + count
    uint32_t magic = HIST_MAGIC;
    f.write((uint8_t*)&magic, 4);
    uint16_t cnt = (uint16_t)buf.size();
    f.write((uint8_t*)&cnt, 2);

    // Write each point as packed struct
    for (size_t i = 0; i < buf.size(); i++) {
        const SensorData& d = buf.at(i);
        HistPoint hp;
        hp.temp = d.temp; hp.hum = d.hum;
        hp.co2  = d.co2;  hp.tvoc = d.tvoc; hp.aqi = d.aqi;
        // Convert millis timestamp to HH:MM string via d.ts
        uint32_t sec = d.ts / 1000;
        snprintf(hp.time, 6, "%02d:%02d",
                 (int)((sec / 3600) % 24), (int)((sec / 60) % 60));
        f.write((uint8_t*)&hp, sizeof(HistPoint));
    }
    f.close();
    DBGF("[SD] History saved: %d points\n", cnt);
}

bool loadHistory(RingBuffer<SensorData, DATA_BUFFER_SIZE>& buf) {
    if (!_sdOK || !SD.exists(HIST_FILE)) return false;
    File f = SD.open(HIST_FILE, FILE_READ);
    if (!f) return false;

    uint32_t magic = 0;
    f.read((uint8_t*)&magic, 4);
    if (magic != HIST_MAGIC) { f.close(); DBGLN("[SD] History: bad magic"); return false; }

    uint16_t cnt = 0;
    f.read((uint8_t*)&cnt, 2);
    if (cnt > DATA_BUFFER_SIZE) cnt = DATA_BUFFER_SIZE;

    uint32_t loaded = 0;
    for (uint16_t i = 0; i < cnt; i++) {
        HistPoint hp;
        if (f.read((uint8_t*)&hp, sizeof(HistPoint)) != sizeof(HistPoint)) break;
        SensorData d;
        d.temp = hp.temp; d.hum = hp.hum;
        d.co2  = hp.co2;  d.tvoc = hp.tvoc; d.aqi = hp.aqi;
        d.ts   = 0;
        buf.push(d);
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
