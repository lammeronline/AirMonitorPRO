#include "oled_display.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace OLEDDisplay {

static Adafruit_SSD1306 _disp(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool    _enabled  = true;
static bool    _initOK   = false;
static uint8_t _page     = 0;
static uint32_t _lastFlip = 0;

// -------------------------------------------------------
static void _drawPage0(const SensorData& d, const SystemStatus& s) {
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    // Title bar
    _disp.setCursor(0, 0);
    _disp.print("AIR MONITOR PRO");
    _disp.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // Two columns layout
    _disp.setCursor(0, 13);
    _disp.setTextSize(1);
    _disp.print("TEMP:");
    _disp.setTextSize(2);
    _disp.setCursor(38, 11);
    _disp.printf("%.1f", d.temp);
    _disp.setTextSize(1);
    _disp.print(" C");

    _disp.setCursor(0, 30);
    _disp.setTextSize(1);
    _disp.print("HUM: ");
    _disp.setTextSize(2);
    _disp.setCursor(38, 28);
    _disp.printf("%.1f", d.hum);
    _disp.setTextSize(1);
    _disp.print(" %");

    _disp.setCursor(0, 47);
    _disp.setTextSize(1);
    _disp.print("CO2: ");
    _disp.printf("%4d ppm", d.co2);

    _disp.setCursor(0, 57);
    _disp.print("AQI: ");
    // AQI bar
    for (int i = 0; i < 5; i++) {
        if (i < d.aqi)
            _disp.fillRect(32 + i * 8, 57, 6, 7, SSD1306_WHITE);
        else
            _disp.drawRect(32 + i * 8, 57, 6, 7, SSD1306_WHITE);
    }

    _disp.display();
}

static void _drawPage1(const SensorData& d, const SystemStatus& s) {
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    _disp.setCursor(0, 0);
    _disp.print("NETWORK / STATUS");
    _disp.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    _disp.setCursor(0, 12);
    _disp.print("IP:   "); _disp.print(s.ip);

    _disp.setCursor(0, 22);
    _disp.print("WiFi: "); _disp.printf("%d dBm", s.rssi);

    _disp.setCursor(0, 32);
    _disp.print("Time: "); _disp.print(s.time_str);

    _disp.setCursor(0, 42);
    _disp.print("Date: "); _disp.print(s.date_str);

    // Status icons row
    _disp.setCursor(0, 54);
    _disp.printf("SD:%s RTC:%s ENS:%s",
        s.sd_ok  ? "O" : "X",
        s.rtc_ok ? "O" : "X",
        s.ens_ok ? "O" : "X");

    _disp.display();
}

// -------------------------------------------------------
void begin() {
    if (_disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        _initOK = true;
        _disp.clearDisplay();
        _disp.setTextSize(1);
        _disp.setTextColor(SSD1306_WHITE);
        _disp.setCursor(20, 20);
        _disp.print("Air Monitor PRO");
        _disp.setCursor(35, 35);
        _disp.print("Starting...");
        _disp.display();
        DBGLN("[OLED] SSD1306 OK");
    } else {
        DBGLN("[OLED] SSD1306 FAIL");
    }
}

void loop(const SensorData& d, const SystemStatus& s) {
    if (!_initOK || !_enabled) return;

    uint32_t now = millis();
    if (now - _lastFlip < OLED_SWITCH_INTERVAL) return;
    _lastFlip = now;

    if (_page == 0) {
        _drawPage0(d, s);
        _page = 1;
    } else {
        _drawPage1(d, s);
        _page = 0;
    }
}

void enable(bool on) {
    _enabled = on;
    if (!on && _initOK) {
        _disp.ssd1306_command(SSD1306_DISPLAYOFF);
    } else if (on && _initOK) {
        _disp.ssd1306_command(SSD1306_DISPLAYON);
    }
}

} // namespace OLEDDisplay
