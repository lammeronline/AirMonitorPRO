#pragma once
#include <Arduino.h>

// ============================================================
//  WiFi Manager
//  – Manages AP (setup) mode and STA (normal) mode
//  – Saves credentials to NVS (Preferences)
// ============================================================

namespace WifiManager {

    enum class Mode { NONE, AP, STA };

    void    begin();               // call once in setup()
    void    loop();                // call every loop()
    Mode    currentMode();
    bool    isConnected();
    bool    isStableConnected(uint32_t minStableMs = 0);

    // Scan & return JSON array string of found networks
    String  scanNetworks();

    // Save new credentials and reboot into STA
    void    saveCredentials(const String& ssid, const String& pass);

    // Force reset — clear NVS and go back to AP mode
    void    factoryReset();

    // Return current RSSI (STA mode)
    int8_t  getRSSI();

    // Return local IP as string
    String  getIP();
}
