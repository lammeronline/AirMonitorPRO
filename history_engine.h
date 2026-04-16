#pragma once
// ============================================================
//  history_engine.h — Template helpers for ring-buffer history
//
//  Must be included ONLY from AirMonitorPRO.ino (not from .cpp
//  files) because Arduino's build system compiles .ino as C++
//  without auto-prototype generation, which makes templates safe.
// ============================================================
#include "data_types.h"
#include <time.h>

// ── Accumulator ───────────────────────────────────────────
struct HistoryAccumulator {
    bool     active       = false;
    uint32_t bucket_start = 0;
    uint16_t count        = 0;
    float    temp_sum     = 0.0f;
    float    hum_sum      = 0.0f;
    uint32_t co2_sum      = 0;
    uint32_t tvoc_sum     = 0;
};

// ── Helpers ───────────────────────────────────────────────
inline void histStartBucket(HistoryAccumulator& acc,
                             uint32_t bucketStart,
                             const SensorData& d) {
    acc.active       = true;
    acc.bucket_start = bucketStart;
    acc.count        = 1;
    acc.temp_sum     = d.temp;
    acc.hum_sum      = d.hum;
    acc.co2_sum      = d.co2;
    acc.tvoc_sum     = d.tvoc;
}

template<size_t N>
void histFlushBucket(HistoryAccumulator& acc,
                     RingBuffer<HistoryPoint, N>& out) {
    if (!acc.active || acc.count == 0) return;
    HistoryPoint p;
    p.ts   = acc.bucket_start;
    p.temp = acc.temp_sum / acc.count;
    p.hum  = acc.hum_sum  / acc.count;
    p.co2  = (uint16_t)(acc.co2_sum  / acc.count);
    p.tvoc = (uint16_t)(acc.tvoc_sum / acc.count);
    out.push(p);
}

template<size_t N>
bool histUpdateBucket(HistoryAccumulator& acc,
                      uint32_t bucketSec,
                      uint32_t epochSec,
                      const SensorData& d,
                      RingBuffer<HistoryPoint, N>& out) {
    if (bucketSec == 0 || epochSec == 0) return false;
    const uint32_t bs = epochSec - (epochSec % bucketSec);
    if (!acc.active) { histStartBucket(acc, bs, d); return false; }
    if (acc.bucket_start != bs) {
        histFlushBucket(acc, out);
        histStartBucket(acc, bs, d);
        return true;
    }
    acc.count++;
    acc.temp_sum += d.temp;
    acc.hum_sum  += d.hum;
    acc.co2_sum  += d.co2;
    acc.tvoc_sum += d.tvoc;
    return false;
}

// ── Label formatter ───────────────────────────────────────
inline String histFmtLabel(uint32_t ts, const String& range) {
    time_t raw = (time_t)ts;
    struct tm t;
    localtime_r(&raw, &t);
    char buf[24];
    if      (range == "24h") snprintf(buf, sizeof(buf), "%02d:%02d",
                                      t.tm_hour, t.tm_min);
    else if (range == "7d")  snprintf(buf, sizeof(buf), "%02d.%02d %02d:00",
                                      t.tm_mday, t.tm_mon + 1, t.tm_hour);
    else                     snprintf(buf, sizeof(buf), "%02d.%02d %02d:%02d",
                                      t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
    return String(buf);
}

// ── JSON builder ──────────────────────────────────────────
template<size_t N>
String histBuildJSON(const RingBuffer<HistoryPoint, N>& buf,
                     const String& range,
                     size_t& lastPts,
                     const size_t HTTP_POINTS = 360) {
    const size_t total = buf.size();
    size_t step = 1, n = total;
    if (n > HTTP_POINTS) {
        step = (n + HTTP_POINTS - 1) / HTTP_POINTS;
        n    = (n + step - 1) / step;
    }

    String j;
    j.reserve(n * 60 + 128);
    j  = "{\"range\":\""; j += range;
    j += "\",\"n\":";    j += n;
    j += ",\"labels\":[";

    // Build labels
    for (size_t i = 0; i < total; i += step) {
        if (i > 0) j += ',';
        j += '"'; j += histFmtLabel(buf.at(i).ts, range); j += '"';
    }
    j += "],\"temp\":[";

    // Build temp
    for (size_t i = 0; i < total; i += step) {
        if (i > 0) j += ',';
        j += String(buf.at(i).temp, 1);
    }
    j += "],\"hum\":[";

    // Build humidity
    for (size_t i = 0; i < total; i += step) {
        if (i > 0) j += ',';
        j += String(buf.at(i).hum, 1);
    }
    j += "],\"co2\":[";

    // Build CO2
    for (size_t i = 0; i < total; i += step) {
        if (i > 0) j += ',';
        j += String(buf.at(i).co2);
    }
    j += "],\"tvoc\":[";

    // Build TVOC
    for (size_t i = 0; i < total; i += step) {
        if (i > 0) j += ',';
        j += String(buf.at(i).tvoc);
    }
    j += "]}";

    lastPts = n;
    return j;
}
