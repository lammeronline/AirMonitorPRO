#pragma once
#include "data_types.h"

// ============================================================
//  MQTT Module
//  Library: PubSubClient (Nick O'Leary) — install via Library Manager
//
//  Publishes to:
//    <topic>/temp   <topic>/hum   <topic>/co2
//    <topic>/tvoc   <topic>/aqi   <topic>/json  (full payload)
//
//  Subscribes to:
//    <topic>/cmd    → "reboot", "reset"
// ============================================================

namespace MQTTModule {

    void  begin();
    void  loop();

    // Call when sensor data is ready
    void  publish(const SensorData& d, const SystemStatus& s);

    bool  isConnected();
    bool  isEnabled();

    // Reload settings from NVS and reconnect
    void  reload();
}
