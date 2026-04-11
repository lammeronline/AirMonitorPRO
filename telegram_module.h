#pragma once
#include "data_types.h"

// ============================================================
//  Telegram Bot Module
//  • /status /report /thresholds /reboot /help commands
//  • Auto-alerts: CO2, AQI, Temp, Humidity thresholds
//  • Thresholds stored in NVS, editable via web UI
// ============================================================

namespace TelegramModule {

    struct ThresholdConfig {
        uint16_t co2_ppm;    // default 1500
        uint8_t  aqi_level;  // default 3
        float    temp_hi;    // default 30.0 °C
        float    hum_hi;     // default 75.0 %
    };

    void            begin();
    void            loop(const SensorData& d, const SystemStatus& s);
    bool            sendMessage(const String& text);
    bool            isEnabled();

    // Called from web_server_module when settings are saved
    void            saveThresholds(uint16_t co2, uint8_t aqi,
                                   float tHi, float hHi);
    ThresholdConfig getThresholds();
}
