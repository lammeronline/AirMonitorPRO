#pragma once
#include <Arduino.h>

namespace RuntimeSettings {

    struct HistoryConfig {
        uint16_t csv_interval_sec;
        uint16_t hist24_interval_min;
        uint16_t hist7_interval_min;
        uint16_t hist30_interval_min;
    };

    void begin();
    void reload();
    HistoryConfig get();
}
