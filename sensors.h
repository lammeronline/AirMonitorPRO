#pragma once
#include "data_types.h"

// ============================================================
//  Sensors — SparkFun ENS160 + AHT20/21
// ============================================================

namespace Sensors {

    void    begin();
    void    loop();

    const SensorData& latest();

    bool    ensOK();
    bool    ahtOK();
    uint8_t ensStatus();        // 0=OK 1=Warmup 2=InitStartup 3=Invalid
    bool    ensWarmingUp();

    // Baseline compensation reset — forces ENS160 to recalibrate
    // Call when air is known-clean (window open, outdoors, etc.)
    bool    calibrateBaseline();
    String  calibrateStatusMsg();  // human-readable last calibration result

    // Reload AHT21 offsets from NVS (call after saving settings)
    void    reloadOffsets();
}
