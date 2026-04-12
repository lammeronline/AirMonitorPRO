#include "runtime_settings.h"
#include "config.h"
#include <Preferences.h>

namespace RuntimeSettings {

static HistoryConfig _cfg = {
    CSV_LOG_INTERVAL_DEFAULT_SEC,
    HIST_24H_INTERVAL_DEFAULT_MIN,
    HIST_7D_INTERVAL_DEFAULT_MIN,
    HIST_30D_INTERVAL_DEFAULT_MIN
};

static uint16_t _clamp(uint16_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void reload() {
    Preferences p;
    p.begin("airmon", true);
    _cfg.csv_interval_sec    = _clamp((uint16_t)p.getInt("csv_int_s",   CSV_LOG_INTERVAL_DEFAULT_SEC), 30, 3600);
    _cfg.hist24_interval_min = _clamp((uint16_t)p.getInt("hist24_min", HIST_24H_INTERVAL_DEFAULT_MIN), 5, 60);
    _cfg.hist7_interval_min  = _clamp((uint16_t)p.getInt("hist7_min",  HIST_7D_INTERVAL_DEFAULT_MIN), 30, 720);
    _cfg.hist30_interval_min = _clamp((uint16_t)p.getInt("hist30_min", HIST_30D_INTERVAL_DEFAULT_MIN), 60, 1440);
    p.end();
}

void begin() {
    reload();
}

HistoryConfig get() {
    return _cfg;
}

} // namespace RuntimeSettings
