#pragma once
#include "config.h"
#include "data_types.h"

// ============================================================
//  SD Logger
//  • CSV logging every 60s  → /logs/YYYY-MM-DD.csv
//  • History cache          → /cache/history.bin  (boot restore)
//  • CSV export             → served via /api/export?date=
// ============================================================

namespace SDLogger {

    void  begin();
    void  loop(const SensorData& d, const String& datetime, const String& dateOnly);
    bool  isOK();

    // SD usage stats
    void  getUsage(uint32_t& usedMB, uint32_t& totalMB, uint8_t& pct);

    // History persistence (ring buffer ↔ SD binary cache)
    void  saveHistory(const RingBuffer<SensorData, DATA_BUFFER_SIZE>& buf,
                      const String& dateOnly);
    bool  loadHistory(RingBuffer<SensorData, DATA_BUFFER_SIZE>& buf);

    // Export: stream one day's CSV file; returns false if not found
    bool  streamExport(const String& date, void* serverPtr);

    // List available log dates (for export picker)
    String listLogDates();
}
