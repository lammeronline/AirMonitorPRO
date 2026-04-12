#pragma once

// ============================================================
//  Air Monitor PRO — Central Config
//  ESP32 WROOM-32
// ============================================================

// ---------- Debug -------------------------------------------
#define DEBUG_MODE          1
#define FORCE_FACTORY_RESET 0

#if DEBUG_MODE
  #define DBG(x)   Serial.print(x)
  #define DBGLN(x) Serial.println(x)
  #define DBGF(...)Serial.printf(__VA_ARGS__)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBGF(...)
#endif

// ---------- Firmware ----------------------------------------
#define FW_VERSION  "1.0.0"
#define DEVICE_NAME "AirMonitor PRO"

// ---------- I2C pins (OLED, ENS160, AHT21, DS3231) ----------
#define I2C_SDA 21
#define I2C_SCL 22

// ---------- SD card (SPI) -----------------------------------
#define SD_CS    5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CLK  18

// ---------- OLED SSD1306 ------------------------------------
#define OLED_WIDTH  128
#define OLED_HEIGHT  64
#define OLED_ADDR   0x3C

// ---------- WiFi AP -----------------------------------------
#define AP_SSID     "AirMonitor_SETUP"
#define AP_PASSWORD ""           // open network
#define AP_IP_STR   "192.168.4.1"

// ---------- Timing (ms) -------------------------------------
#define SENSOR_READ_INTERVAL   10000UL   // sensor poll
#define OLED_SWITCH_INTERVAL    5000UL   // page flip
#define WS_BROADCAST_INTERVAL   2000UL   // WebSocket push
#define NTP_SYNC_INTERVAL    3600000UL   // NTP re-sync

// ---------- ENS160 warm-up ----------------------------------
#define ENS160_WARMUP_MS      180000UL   // 3 minutes

// ---------- Data ring buffer ---------------------------------
#define DATA_BUFFER_SIZE 1440            // 24 h @ 1/min

// ---------- Runtime history/logging defaults -----------------
#define CSV_LOG_INTERVAL_DEFAULT_SEC      60
#define HIST_24H_INTERVAL_DEFAULT_MIN      5
#define HIST_7D_INTERVAL_DEFAULT_MIN      60
#define HIST_30D_INTERVAL_DEFAULT_MIN    360

// ---------- History buffer capacities ------------------------
#define HISTORY_24H_CAP 288              // 24h @ 5 min
#define HISTORY_7D_CAP  336              // 7d  @ 30 min
#define HISTORY_30D_CAP 720              // 30d @ 1 hour

// ---------- NTP ---------------------------------------------
#define NTP_SERVER1  "pool.ntp.org"
#define NTP_SERVER2  "time.google.com"
#define TZ_OFFSET_SEC 10800              // UTC+3 (Ukraine)

// ---------- SD paths ----------------------------------------
#define LOG_DIR  "/logs"

// ---------- Ports -------------------------------------------
#define WEB_PORT 80
#define WS_PORT  81
