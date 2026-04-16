#pragma once
#include "data_types.h"

// ============================================================
//  Telegram Bot Module
//  • /status /report /thresholds /reboot /help commands
//  • Auto-alerts: CO2, TVOC, AQI, Temp, Humidity thresholds
//  • Thresholds stored in NVS, editable via web UI
// ============================================================

namespace TelegramModule {

    struct ThresholdConfig {
        uint16_t co2_ppm;       // default 1500
        uint16_t tvoc_ppb;      // default 500
        uint8_t  aqi_level;     // default 3
        float    temp_hi;       // default 30.0 °C
        float    hum_hi;        // default 75.0 %
        uint8_t  cooldown_min;  // default 5 min
    };

    void            begin();
    void            init();     // Load settings from NVS (called once in setup)
    void            reload();   // Reload settings from NVS
    void            loop(const SensorData& d, const SystemStatus& s);
    bool            sendMessage(const String& text);
    bool            isEnabled();

    void            saveThresholds(uint16_t co2, uint16_t tvoc, uint8_t aqi,
                                   float tHi, float hHi, uint8_t cooldownMin);
    ThresholdConfig getThresholds();
}
