#pragma once

// ============================================================
//  Air Monitor PRO — Central Config
//  ESP32 WROOM-32
// ============================================================

// ---------- Factory reset -----------------------------------
// Установи в 1, прошей, затем верни в 0 и прошей ещё раз.
// Стирает все настройки NVS (WiFi, MQTT, Telegram, пороги).
#define FORCE_FACTORY_RESET 0

// ---------- Runtime debug -----------------------------------
// Включается/выключается из веб-интерфейса без перекомпиляции.
// Переключатель Debug Mode в System → Options.
extern bool g_debug_enabled;
#define DBG(x)    do { if (g_debug_enabled) Serial.print(x);    } while(0)
#define DBGLN(x)  do { if (g_debug_enabled) Serial.println(x);  } while(0)
#define DBGF(...) do { if (g_debug_enabled) Serial.printf(__VA_ARGS__); } while(0)

// ---------- Firmware ----------------------------------------
#define FW_VERSION  "1.0.0"
#define DEVICE_NAME "AirMonitor PRO"

// ---------- Serial ------------------------------------------
#define SERIAL_BAUD         115200

// ---------- I2C pins (OLED, ENS160, AHT21, DS3231) ----------
#define I2C_SDA 21
#define I2C_SCL 22

// ---------- I2C addresses -----------------------------------
#define OLED_ADDR           0x3C
#define ENS160_ADDR_LOW     0x52    // ADDR pin → GND
#define ENS160_ADDR_HIGH    0x53    // ADDR pin → VCC

// ---------- SD card (SPI) -----------------------------------
#define SD_CS    5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CLK  18

// ---------- OLED SSD1306 ------------------------------------
#define OLED_WIDTH  128
#define OLED_HEIGHT  64

// ---------- WiFi AP -----------------------------------------
#define AP_SSID         "AirMonitor_SETUP"
#define AP_PASSWORD     ""              // open network
#define AP_IP           192, 168, 4, 1  // используется как IPAddress(AP_IP)
#define AP_IP_STR       "192.168.4.1"
#define AP_SUBNET       255, 255, 255, 0
#define AP_DNS_PORT     53

// ---------- Timing (ms) -------------------------------------
#define SENSOR_READ_INTERVAL   10000UL
#define OLED_SWITCH_INTERVAL    5000UL
#define WS_BROADCAST_INTERVAL   2000UL
#define NTP_SYNC_INTERVAL    3600000UL

// ---------- ENS160 warm-up ----------------------------------
#define ENS160_WARMUP_MS      180000UL   // 3 minutes

// ---------- Runtime history/logging defaults ----------------
#define CSV_LOG_INTERVAL_DEFAULT_SEC      60
#define HIST_24H_INTERVAL_DEFAULT_MIN      5
#define HIST_7D_INTERVAL_DEFAULT_MIN      60
#define HIST_30D_INTERVAL_DEFAULT_MIN    360

// ---------- History buffer capacities -----------------------
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

// ---------- MQTT defaults -----------------------------------
#define MQTT_DEFAULT_PORT   1883
#define MQTT_DEFAULT_TOPIC  "airmonitor"

// ---------- Alert threshold defaults ------------------------
#define ALERT_DEFAULT_CO2_PPM      1500
#define ALERT_DEFAULT_TVOC_PPB      500
#define ALERT_DEFAULT_AQI             3
#define ALERT_DEFAULT_TEMP_HI      30.0f
#define ALERT_DEFAULT_HUM_HI       75.0f
#define ALERT_DEFAULT_COOLDOWN_MIN    5   // minutes between repeated alerts
