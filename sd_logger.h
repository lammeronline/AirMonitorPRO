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
    void  loop(const SensorData& d, const String& datetime, const String& dateOnly,
               uint32_t epochSec, bool timeValid);
    bool  isOK();

    // SD usage stats
    void  getUsage(uint32_t& usedMB, uint32_t& totalMB, uint8_t& pct);

    // History persistence (ring buffer ↔ SD binary cache)
    void  saveHistory(const RingBuffer<HistoryPoint, HISTORY_24H_CAP>& hist24,
                      const RingBuffer<HistoryPoint, HISTORY_7D_CAP>& hist7,
                      const RingBuffer<HistoryPoint, HISTORY_30D_CAP>& hist30);
    bool  loadHistory(RingBuffer<HistoryPoint, HISTORY_24H_CAP>& hist24,
                      RingBuffer<HistoryPoint, HISTORY_7D_CAP>& hist7,
                      RingBuffer<HistoryPoint, HISTORY_30D_CAP>& hist30);

    // Export: stream one day's CSV file; returns false if not found
    bool  streamExport(const String& date, void* serverPtr);

    // List available log dates (for export picker)
    String listLogDates();
}
