#pragma once
#include "data_types.h"

// ============================================================
//  OLED SSD1306 — 2 rotating pages
// ============================================================

namespace OLEDDisplay {

    void begin();
    void loop(const SensorData& d, const SystemStatus& s);

    void enable(bool on);    // for power saving
}
