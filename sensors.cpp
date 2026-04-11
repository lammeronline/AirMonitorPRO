#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include "SparkFun_ENS160.h"
#include "SparkFun_Qwiic_Humidity_AHT20.h"

namespace Sensors {

static SparkFun_ENS160 _ens;
static AHT20           _aht;

static SensorData _latest;
static bool       _ensOK      = false;
static bool       _ahtOK      = false;
static uint8_t    _ensStatus  = 2;       // Initial Start-up
static uint32_t   _lastRead   = 0;
static uint32_t   _startMs    = 0;
static String     _calMsg     = "Not calibrated yet";

// ── Init ──────────────────────────────────────────────────
void begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    _startMs = millis();

    if (_aht.begin()) {
        _ahtOK = true;
        DBGLN("[Sensors] AHT21 OK");
    } else {
        DBGLN("[Sensors] AHT21 FAIL");
    }

    if (_ens.begin()) {
        _ens.setOperatingMode(SFE_ENS160_STANDARD);
        _ensOK = true;
        _ensStatus = 2;
        DBGLN("[Sensors] ENS160 OK");
    } else {
        DBGLN("[Sensors] ENS160 FAIL");
        _ensStatus = 3;
    }
}

// ── Non-blocking poll ─────────────────────────────────────
void loop() {
    uint32_t now = millis();
    if (now - _lastRead < SENSOR_READ_INTERVAL) return;
    _lastRead = now;

    // AHT21
    if (_ahtOK && _aht.isConnected()) {
        float t = _aht.getTemperature();
        float h = _aht.getHumidity();
        if (t > -40.0f && t < 85.0f)  _latest.temp = t;
        if (h >   0.0f && h < 100.0f) _latest.hum  = h;
        if (_ensOK) {
            _ens.setTempCompensation(_latest.temp);
            _ens.setRHCompensation(_latest.hum);
        }
    }

    // ENS160 — read VALIDITY flags from STATUS register
    if (_ensOK) {
        uint8_t flags = _ens.getFlags();
        _ensStatus = (flags >> 2) & 0x03;  // bits [3:2] = VALIDITY

        if (_ensStatus == 0 && _ens.checkDataStatus()) {
            uint16_t co2  = _ens.getECO2();
            uint16_t tvoc = _ens.getTVOC();
            uint8_t  aqi  = _ens.getAQI();
            if (co2  >= 400 && co2  <= 65000) _latest.co2  = co2;
            if (tvoc <= 65000)                 _latest.tvoc = tvoc;
            if (aqi  >= 1   && aqi  <= 5)      _latest.aqi  = aqi;
        }
    }

    _latest.ts = now;

    DBGF("[Sensors] T=%.1f RH=%.1f CO2=%d TVOC=%d AQI=%d STATUS=%d\n",
         _latest.temp, _latest.hum, _latest.co2,
         _latest.tvoc, _latest.aqi, _ensStatus);
}

// ── Baseline calibration ──────────────────────────────────
// ENS160 calibration: cycle through RESET → IDLE → STANDARD
// This clears the sensor's internal accumulated baseline and
// forces it to re-learn from the current (assumed-clean) air.
bool calibrateBaseline() {
    if (!_ensOK) {
        _calMsg = "ENS160 not available";
        return false;
    }
    DBGLN("[Sensors] Calibrating ENS160 baseline...");

    _ens.setOperatingMode(SFE_ENS160_RESET);
    delay(100);
    _ens.setOperatingMode(SFE_ENS160_IDLE);
    delay(100);
    _ens.setOperatingMode(SFE_ENS160_STANDARD);
    delay(100);

    // After reset the sensor enters Initial Start-up again (~3 min)
    _ensStatus = 2;
    _startMs   = millis();

    char buf[64];
    snprintf(buf, sizeof(buf),
             "Done at %.1f°C / %.1f%% RH — re-warming up",
             _latest.temp, _latest.hum);
    _calMsg = String(buf);
    DBGLN("[Sensors] Calibration: " + _calMsg);
    return true;
}

String calibrateStatusMsg()    { return _calMsg; }
const SensorData& latest()     { return _latest; }
bool    ensOK()                { return _ensOK; }
bool    ahtOK()                { return _ahtOK; }
uint8_t ensStatus()            { return _ensStatus; }
bool    ensWarmingUp()         { return _ensOK && (_ensStatus != 0); }

} // namespace Sensors
