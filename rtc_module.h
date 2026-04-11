#pragma once
#include <Arduino.h>

// ============================================================
//  RTC DS3231 + NTP sync
// ============================================================

namespace RTCModule {

    void   begin();
    void   loop();                       // handles NTP sync timer

    bool   isOK();
    bool   hasValidTime();
    uint32_t getEpoch();
    String getTimeString();              // "HH:MM:SS"
    String getDateString();             // "YYYY-MM-DD"
    String getDateTimeString();         // "YYYY-MM-DD HH:MM:SS"

    // Sync RTC from NTP (call when WiFi ready)
    void   syncFromNTP();
}
