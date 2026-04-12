#pragma once
#include <Arduino.h>

// ============================================================
//  Shared data types for Air Monitor PRO
// ============================================================

// ---------- One sensor reading ------------------------------
struct SensorData {
    float    temp   = 0.0f;   // °C
    float    hum    = 0.0f;   // %
    uint16_t co2    = 400;    // ppm  (eCO2)
    uint16_t tvoc   = 0;      // ppb
    uint8_t  aqi    = 1;      // 1-5
    uint32_t ts     = 0;      // unix epoch seconds
};

struct HistoryPoint {
    uint32_t ts   = 0;        // unix epoch bucket start
    float    temp = 0.0f;
    float    hum  = 0.0f;
    uint16_t co2  = 400;
    uint16_t tvoc = 0;
};

// ---------- System status flags -----------------------------
struct SystemStatus {
    bool sd_ok      = false;
    bool rtc_ok     = false;
    bool ens_ok     = false;
    bool aht_ok     = false;
    int8_t  rssi    = 0;
    String  ip      = "---";
    String  time_str = "--:--:--";
    String  date_str = "----/--/--";
    uint32_t uptime = 0;       // seconds
    bool ens_warmup = true;
    uint32_t sd_used_mb  = 0;
    uint32_t sd_total_mb = 0;
    uint8_t  sd_pct      = 0;    // still warming up
    uint32_t hist24_rev  = 0;
    uint32_t hist7_rev   = 0;
    uint32_t hist30_rev  = 0;
};

// ---------- Ring buffer for history -------------------------
template<typename T, size_t N>
class RingBuffer {
public:
    void push(const T& v) {
        _buf[_head] = v;
        _head = (_head + 1) % N;
        if (_count < N) _count++;
    }
    size_t size()  const { return _count; }
    size_t cap()   const { return N; }
    // oldest → newest
    T at(size_t i) const {
        size_t idx = (_head + N - _count + i) % N;
        return _buf[idx];
    }
private:
    T       _buf[N];
    size_t  _head  = 0;
    size_t  _count = 0;
};
