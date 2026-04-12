# 🌿 Air Monitor PRO

> ESP32-based indoor air quality monitor with a real-time web dashboard, Telegram alerts, MQTT, SD logging, and OTA updates.

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue)
![Arduino](https://img.shields.io/badge/Arduino-Framework-teal)
![License](https://img.shields.io/badge/License-MIT-green)

---

## Features

| Category | Details |
|---|---|
| **Sensors** | ENS160 (eCO₂, TVOC, AQI 1–5), AHT21 (temperature, humidity) |
| **Display** | SSD1306 OLED 128×64 — two auto-rotating pages |
| **Web UI** | Real-time WebSocket dashboard, charts (24h / 7d / 30d), dark theme |
| **Telegram** | Alerts for CO₂, AQI, temperature, humidity; bot commands `/status /report /thresholds /reboot /help` |
| **MQTT** | Per-topic + JSON payload publish, command subscription (`reboot`, `reset`) |
| **SD logging** | CSV files per day (`/logs/YYYY-MM-DD.csv`), streamed HTTP export |
| **OTA** | Drag-and-drop `.bin` firmware update via browser with confirmation dialog showing current vs new version |
| **RTC** | DS3231 hardware clock + NTP sync with configurable server and timezone |
| **History persistence** | Ring-buffer snapshots saved to SD every 5 min, restored on boot |
| **Settings** | Full in-browser config: WiFi, device name/hostname, MQTT, Telegram, NTP/TZ, alert thresholds, logging intervals |

---

## Hardware

| Component | Model | Interface |
|---|---|---|
| Microcontroller | ESP32 WROOM-32 | — |
| Air quality | ScioSense ENS160 | I²C 0x52/0x53 |
| Temp/humidity | AHT21 | I²C 0x38 |
| RTC | DS3231 | I²C 0x57 |
| OLED display | SSD1306 128×64 | I²C 0x3C |
| Storage | microSD card | SPI |

### Wiring

```
ESP32        ENS160 / AHT21 / DS3231 / OLED
─────────────────────────────────────────────
GPIO 21  →   SDA  (I²C shared bus)
GPIO 22  →   SCL  (I²C shared bus)
3.3V     →   VCC  (all I²C devices)
GND      →   GND  (all I²C devices)

ESP32        SD card module (SPI)
─────────────────────────────────
GPIO  5  →   CS
GPIO 23  →   MOSI
GPIO 19  →   MISO
GPIO 18  →   CLK
3.3V     →   VCC
GND      →   GND
```

---

## Firmware Structure

```
AirMonitorPRO/
├── AirMonitorPRO.ino       Main sketch — setup/loop, history engine
├── config.h                Central config: pins, timing, NTP defaults
├── data_types.h            SensorData, SystemStatus, HistoryPoint, RingBuffer<>
├── sensors.cpp/.h          ENS160 + AHT21 driver, baseline calibration
├── wifi_manager.cpp/.h     STA/AP mode, reconnect, hostname, NVS credentials
├── web_server_module.cpp   HTTP routes, WebSocket, REST API
├── web_ui.h                Single-file embedded web app (HTML/CSS/JS)
├── ota_module.cpp/.h       OTA drag-and-drop with confirmation modal
├── mqtt_module.cpp/.h      PubSubClient wrapper, pre-built topics
├── telegram_module.cpp/.h  Telegram Bot API (TLS, local WiFiClientSecure)
├── rtc_module.cpp/.h       DS3231 + NTP sync (server/TZ from NVS)
├── sd_logger.cpp/.h        CSV logging, binary history persistence
├── oled_display.cpp/.h     SSD1306 two-page display
└── runtime_settings.cpp/.h History/logging interval config (NVS)
```

---

## Web API

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/` | Main dashboard |
| `GET` | `/api/history?range=24h\|7d\|30d` | History JSON |
| `GET` | `/api/export?date=YYYY-MM-DD` | Stream CSV from SD |
| `GET` | `/api/export` | List available log dates |
| `GET` | `/api/thresholds` | Get alert thresholds |
| `POST` | `/api/thresholds` | Set alert thresholds |
| `POST` | `/api/settings` | Save settings (WiFi, MQTT, Telegram, NTP, …) |
| `POST` | `/api/wifi` | Save WiFi credentials → reboot |
| `GET` | `/api/scan` | Scan nearby WiFi networks (JSON) |
| `GET` | `/api/tg_test` | Send Telegram test message |
| `POST` | `/api/calibrate` | Reset ENS160 baseline |
| `POST` | `/api/reboot` | Reboot device |
| `POST` | `/api/reset` | Factory reset (clears NVS) → reboot |
| `GET` | `/update` | OTA upload page |
| `POST` | `/do_update` | Flash firmware binary |
| `WS` | `ws://<ip>:81/` | Live data + settings WebSocket |

### WebSocket messages

**Client → Device**
```json
{ "cmd": "get_settings" }
```

**Device → Client** (every 2 s)
```json
{
  "type": "data",
  "temp": 22.4, "hum": 48.1, "co2": 812, "tvoc": 45, "aqi": 2,
  "rssi": -58, "uptime": 3600, "ip": "192.168.1.42",
  "time_short": "14:32:10", "date": "2026-04-12",
  "ver": "1.2.0", "mqtt": true, "tg": true,
  "sd": true, "sd_used": 12, "sd_total": 3600, "sd_pct": 0,
  "rtc": true, "ens": true, "aht": true,
  "warmup": false, "warmup_pct": 100,
  "hist24_rev": 5, "hist7_rev": 1, "hist30_rev": 0
}
```

---

## MQTT Topics

Default prefix: `airmonitor` (configurable)

| Topic | Payload | Direction |
|---|---|---|
| `airmonitor/temp` | `22.4` | Publish |
| `airmonitor/hum` | `48.1` | Publish |
| `airmonitor/co2` | `812` | Publish |
| `airmonitor/tvoc` | `45` | Publish |
| `airmonitor/aqi` | `2` | Publish |
| `airmonitor/rssi` | `-58` | Publish |
| `airmonitor/json` | Full JSON object | Publish |
| `airmonitor/status` | `online` (retained) | Publish |
| `airmonitor/cmd` | `reboot` / `reset` | Subscribe |

---

## Telegram Bot Commands

| Command | Description |
|---|---|
| `/status` | Current readings + system info |
| `/report` | JSON dump of current data |
| `/thresholds` | Show current alert thresholds |
| `/reboot` | Reboot the device |
| `/help` | Command list |

Alert notifications fire when readings exceed thresholds (5-minute cooldown per alert type). Configurable via web UI → Settings → Alerts.

---

## First-time Setup

1. Flash the firmware via Arduino IDE or `esptool`.
2. Power on — device starts in **AP mode** (`AirMonitor_SETUP`).
3. Connect to the AP, navigate to `http://192.168.4.1`.
4. Select your WiFi network, enter password, save.
5. Device reboots into **STA mode** — find its IP on your router or serial monitor.
6. Open `http://<device-ip>` → configure Telegram, MQTT, NTP, etc. from the Settings panel.

---

## Required Libraries (Arduino Library Manager)

| Library | Author |
|---|---|
| `SparkFun ENS160 Arduino Library` | SparkFun |
| `SparkFun Qwiic Humidity AHT20` | SparkFun |
| `Adafruit GFX Library` | Adafruit |
| `Adafruit SSD1306` | Adafruit |
| `RTClib` | Adafruit |
| `ArduinoJson` | Benoit Blanchon |
| `PubSubClient` | Nick O'Leary |
| `WebSockets` | Markus Sattler |

ESP32 board package: **esp32 by Espressif** (via Boards Manager)  
Tested with Arduino IDE 2.x, `esp32` core ≥ 3.0.

---

## Configuration (`config.h`)

Key defines — adjust before flashing:

```cpp
#define FW_VERSION      "1.2.0"
#define DEVICE_NAME     "AirMonitor-PRO"   // default hostname

#define NTP_SERVER1     "pool.ntp.org"
#define NTP_SERVER2     "time.google.com"
#define TZ_OFFSET_SEC   10800              // UTC+3 — change for your timezone

#define SENSOR_READ_INTERVAL   10000UL     // ms between sensor reads
#define WS_BROADCAST_INTERVAL   2000UL     // ms between WebSocket pushes
#define ENS160_WARMUP_MS      180000UL     // 3-minute ENS160 warm-up
```

All runtime parameters (NTP server, TZ offset, thresholds, intervals, device name) can also be changed via the web UI without reflashing.

---

## Performance Notes

- **WiFi stability**: `WiFiClientSecure` in `telegram_module` is instantiated as a local variable so mbedTLS buffers (~35 KB) are fully released after every API call, preventing heap fragmentation that previously caused WiFi drops.
- **Heap footprint**: History ring buffers (24h/7d/30d) are statically allocated; JSON building uses `String::reserve()` to avoid reallocation churn.
- **Publish efficiency**: MQTT topic strings are pre-built at connect time; individual field values are formatted with `snprintf` into a stack buffer — no temporary `String` objects per publish.
- **Template history engine**: `_buildHistoryJSON<N>`, `_updateHistory<N>`, `_flushBucket<N>` are a single template implementation shared by all three ring buffers — no code duplication.
- **Date/time**: `getDateString()` / `getTimeString()` are called once per `loop()` iteration and the results reused for SD logging, WebSocket broadcast, and OLED display.

---

## License

MIT — see [LICENSE](LICENSE).
