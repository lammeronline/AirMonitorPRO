#include "rtc_module.h"
#include "config.h"
#include <RTClib.h>       // Adafruit RTClib
#include <WiFi.h>
#include <time.h>

namespace RTCModule {

static RTC_DS3231 _rtc;
static bool       _rtcOK     = false;
static uint32_t   _lastNTPts = 0;
static bool       _ntpSynced = false;

// -------------------------------------------------------
void begin() {
    if (_rtc.begin()) {
        _rtcOK = true;
        if (_rtc.lostPower()) {
            DBGLN("[RTC] Lost power — time invalid, waiting for NTP");
        } else {
            DBGLN("[RTC] DS3231 OK");
        }
    } else {
        DBGLN("[RTC] DS3231 not found — using ESP32 internal clock");
    }
}

void syncFromNTP() {
    if (WiFi.status() != WL_CONNECTED) return;
    DBGLN("[RTC] Syncing from NTP...");

    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER1, NTP_SERVER2);

    struct tm t;
    if (getLocalTime(&t, 5000)) {
        _ntpSynced = true;
        if (_rtcOK) {
            DateTime dt(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                        t.tm_hour, t.tm_min, t.tm_sec);
            _rtc.adjust(dt);
        }
        DBGF("[RTC] NTP sync OK: %04d-%02d-%02d %02d:%02d:%02d\n",
             t.tm_year+1900, t.tm_mon+1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        DBGLN("[RTC] NTP sync FAIL");
    }
}

void loop() {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
        if (!_ntpSynced || (now - _lastNTPts > NTP_SYNC_INTERVAL)) {
            _lastNTPts = now;
            syncFromNTP();
        }
    }
}

bool isOK() { return _rtcOK || _ntpSynced; }

String getTimeString() {
    if (_rtcOK) {
        DateTime dt = _rtc.now();
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 dt.hour(), dt.minute(), dt.second());
        return String(buf);
    }
    struct tm t; char buf[12];
    if (getLocalTime(&t, 0)) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }
    return "--:--:--";
}

String getDateString() {
    if (_rtcOK) {
        DateTime dt = _rtc.now();
        char buf[14];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                 dt.year(), dt.month(), dt.day());
        return String(buf);
    }
    struct tm t; char buf[14];
    if (getLocalTime(&t, 0)) {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                 t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        return String(buf);
    }
    return "----/--/--";
}

String getDateTimeString() {
    return getDateString() + " " + getTimeString();
}

} // namespace RTCModule
