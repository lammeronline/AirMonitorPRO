# 🌿 Air Monitor PRO

> ESP32-based indoor air quality monitor with real-time web dashboard, Telegram alerts, MQTT integration, SD logging, and OTA updates.

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue)
![Arduino](https://img.shields.io/badge/Arduino-Framework-teal)
![PlatformIO](https://img.shields.io/badge/PlatformIO-v6.1-orange)
![Status](https://img.shields.io/badge/Status-Production--Ready-brightgreen)
![License](https://img.shields.io/badge/License-MIT-green)

**Current Version:** 1.0.0  
**Last Updated:** April 2026 — Critical security fixes (DoS protection, JSON validation, race condition patches), optimized logging

---

## 🎯 Features

| Category | Details |
|---|---|
| **Sensors** | ENS160 (eCO₂, TVOC, AQI 1–5), AHT21 (temperature, humidity) with baseline calibration |
| **Display** | SSD1306 OLED 128×64 — two auto-rotating pages with real-time data |
| **Web UI** | Responsive dark-theme dashboard, WebSocket real-time updates, history charts (24h / 7d / 30d) |
| **Telegram Bot** | Alerts + commands (`/status`, `/report`, `/thresholds`, `/reboot`, `/help`) with 5-min cooldown per alert |
| **MQTT** | Per-topic + JSON payload publish, command subscription (`reboot`, `reset`), retained status |
| **SD Logging** | CSV files per day (`/logs/YYYY-MM-DD.csv`), streamed HTTP export, binary history persistence |
| **OTA Firmware** | Drag-and-drop `.bin` via main web interface with confirmation dialog (current vs new version) |
| **RTC** | DS3231 hardware clock + NTP sync with configurable server, timezone, DST handling |
| **History** | Ring-buffer snapshots (24h/7d/30d) saved to SD every 5 min, restored on boot |
| **Settings** | Full in-browser configuration: WiFi, MQTT, Telegram, NTP/TZ, alert thresholds, logging intervals |
| **Security** | `FORCE_FACTORY_RESET` toggle, NVS-persisted settings, Telegram API rate limiting, DoS protection |

---

## ⚡ Recent Improvements (v1.0.0)

- **DoS Protection**: `/reboot` command rate-limited to once per 60 seconds, prevents infinite boot loops
- **JSON Buffer**: Expanded Telegram API response buffer from 256 → 4096 bytes, eliminates `NoMemory` errors
- **Telegram Initialization**: New `TelegramModule::init()` called in setup() to load NVS early, correct startup state
- **Race Condition Fix**: Main loop now snapshots `isEnabled()` state for atomic decision-making
- **JSON Validation**: Malformed Telegram responses safely skipped (no null-pointer crashes)
- **First-Poll Filter**: Startup commands from pre-reboot queue automatically skipped, prevents command re-execution
- **Optimized Logging**: ~90% reduction in Serial port spam while maintaining critical event visibility

---

## 📦 Hardware Requirements

| Component | Model | Interface |
|---|---|---|
| Microcontroller | ESP32 WROOM-32 (240 MHz, 320 KB RAM) | — |
| Air Quality | ScioSense ENS160 | I²C 0x52/0x53 |
| Temp/Humidity | AHT21 | I²C 0x38 |
| Real-Time Clock | DS3231 | I²C 0x57 |
| OLED Display | SSD1306 128×64 | I²C 0x3C |
| SD Card Module | Generic microSD | SPI |
| Power Cable | USB Type-C (data + power) | — |

### Pin Wiring

```
ESP32        I²C Devices (ENS160, AHT21, DS3231, OLED)
─────────────────────────────────────────────────────
GPIO 21  →   SDA (shared bus)
GPIO 22  →   SCL (shared bus)
3.3V     →   VCC (all devices)
GND      →   GND (all devices)

ESP32        SD Card Module (SPI)
─────────────────────────────────
GPIO  5  →   CS
GPIO 23  →   MOSI
GPIO 19  →   MISO
GPIO 18  →   CLK
3.3V     →   VCC
GND      →   GND
```

---

## 🔧 Installation & Setup

### Step 1: Install Arduino IDE 2.x

- Download from https://www.arduino.cc/en/software
- Install on Windows 10/11

### Step 2: Add ESP32 Board Package

1. Open **File → Preferences → Additional Boards Manager URLs**
2. Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Open **Tools → Board → Boards Manager**
4. Search **esp32 by Espressif Systems** → Install (v3.0+)
5. Select **Tools → Board → ESP32 → ESP32 Dev Module**

### Step 3: Install Required Libraries

Open **Tools → Manage Libraries** and install each:

| Library | Author | Min Version |
|---|---|---|
| ArduinoJson | Benoit Blanchon | 6.19.x |
| Adafruit GFX Library | Adafruit | 1.11.x |
| Adafruit SSD1306 | Adafruit | 2.5.x |
| RTClib | Adafruit | 2.0.x |
| WebSockets | Markus Sattler | 2.4.x |
| PubSubClient | Nick O'Leary | 2.8.x |
| SparkFun ENS160 Arduino Library | SparkFun | 1.0.x |
| SparkFun Qwiic Humidity AHT20 | SparkFun | 1.0.x |

> **Tip**: When installing Adafruit SSD1306, click **Install All Dependencies** when prompted.

### Step 4: Configure Compiler Settings

In Arduino IDE, open **Tools** menu and set:

| Parameter | Value |
|---|---|
| **Board** | ESP32 Dev Module |
| **Upload Speed** | 921600 |
| **CPU Frequency** | 240MHz (WiFi/BT) |
| **Flash Frequency** | 80MHz |
| **Flash Mode** | QIO |
| **Flash Size** | **4MB (32Mb)** |
| **Partition Scheme** | **Default 4MB with spiffs** |
| **Core Debug Level** | None |
| **PSRAM** | Disabled |

### Step 5: Project Configuration (Optional)

Edit `config.h` before compiling:

```cpp
// Your timezone (UTC+3 = Moscow/Ukraine; UTC+0 = UK; UTC-5 = EST)
#define TZ_OFFSET_SEC 10800

// Emergency factory reset (set to 1, compile, flash, then set back to 0)
#define FORCE_FACTORY_RESET 0

// Firmware version (displayed on web UI and OLED)
#define FW_VERSION "1.0.0"

// Default device hostname
#define DEVICE_NAME "AirMonitor-PRO"

// Sensor polling intervals (milliseconds)
#define SENSOR_READ_INTERVAL 10000UL
#define WS_BROADCAST_INTERVAL 2000UL
```

> **Note:** Runtime parameters (WiFi credentials, MQTT broker, Telegram token, alert thresholds, etc.) are saved in NVS and changed via web UI without reflashing.

---

## 📤 Flashing Firmware

### Preparation

1. **Connect ESP32 via USB Type-C (data cable, not just charging!)**
2. Check **Tools → Port** for the detected COM port (e.g., `COM3`)
3. If port doesn't appear:
   - Try restarting Arduino IDE
   - Install drivers: `CP210x` or `CH340` (depends on your board variant)

### Upload Process

1. Open the project: **File → Open** → `AirMonitorPRO.ino`
2. Click **Upload** (→ button) or press Ctrl+U
3. On some boards (ESP32-DevKit v4), upload works automatically
4. **On older boards:**
   - Press and hold the **BOOT** button on ESP32
   - In Arduino IDE, click **Upload**
   - When console shows `Connecting....` — release **BOOT**
   - Wait for `Hard resetting via RTS pin...` → flashing complete!

### Verify Flash

After upload succeeds, open **Tools → Serial Monitor** and set baud rate to **115200**. You should see:

```
╔══════════════════════════════╗
║   Air Monitor PRO  v1.0.0   ║
╚══════════════════════════════╝
[SD] Card OK
[Sensors] AHT21 OK
[Sensors] ENS160 OK
[OLED] SSD1306 OK
[RTC] DS3231 OK
[WiFi] Starting AP: AirMonitor_SETUP
[Web] HTTP:80 WS:81
[Main] Setup complete
```

If sensors show `not found` or `FAIL`, check wiring and I²C addresses. For initial testing, missing sensors are non-blocking.

---

## 🌐 First-Time WiFi Setup

### Initial AP Mode

1. When device boots for the first time, it enters **Access Point (AP) mode**
2. From your phone, scan WiFi networks and connect to **`AirMonitor_SETUP`** (no password)
3. Open a web browser and go to **`http://192.168.4.1`**

### Configure Router WiFi

1. In the web UI, click **⚙️ Settings** → **Network** tab
2. Select your home WiFi network from the list
3. Enter WiFi password
4. Click **Connect** → device reboots into **Station (STA) mode**
5. IP address will appear on OLED (page 2) and Serial Monitor

### Find Device IP

- **Option 1:** Check your router's connected devices list
- **Option 2:** Open Serial Monitor → look for `[WiFi] IP: 192.168.x.x`
- **Option 3:** Try `http://airmonitor-pro.local/` (if mDNS is enabled on your router)

---

## 📡 MQTT Setup (Optional)

1. Open **`http://<device-ip>`** in browser
2. Click **⚙️ Settings** → **MQTT** tab
3. Enable the toggle
4. Enter broker address (e.g., `mosquitto.local`, `MQTT.fx`, HiveMQ public broker, etc.)
5. Optionally change topic prefix (default: `airmonitor`)
6. Click **Save & Apply** → device reconnects

### Published Topics

Data publishes every 30 seconds:

```
airmonitor/temp      → 23.5
airmonitor/hum       → 45.2
airmonitor/co2       → 850
airmonitor/tvoc      → 120
airmonitor/aqi       → 2
airmonitor/rssi      → -58
airmonitor/json      → {"temp":23.5,"hum":45.2,...}
airmonitor/status    → online (retained)
```

### Commands (Subscribe)

Publish commands to `airmonitor/cmd`:

```
reboot  → Restart the device
reset   → Factory reset (clears NVS) → reboot
```

---

## 🤖 Telegram Bot Setup

### 1. Create a Bot

1. Open Telegram and search for **@BotFather**
2. Send `/newbot`
3. Give your bot a name and username (must end with `bot`, e.g., `airmonitor_pro_bot`)
4. **Copy the token** (format: `7123456789:AAFxxxxxxxx`) and save it

### 2. Get Your Chat ID

1. Find your newly created bot in Telegram
2. Send `/start` to it
3. Open this URL in browser (replace `<YOUR_TOKEN>`):
   ```
   https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates
   ```
4. Look for `"id":` inside the `"chat"` object — that's your **Chat ID**
5. Save it

### 3. Configure Device

1. Open **`http://<device-ip>`** → **⚙️ Settings** → **Telegram** tab
2. Enable the toggle
3. Paste your **Token** and **Chat ID**
4. Click **Test Send** → you must receive a test message from your bot
5. Click **Save & Apply**

### Bot Commands

Send commands to your bot in Telegram:

| Command | Response |
|---|---|
| `/status` | Current CO₂, AQI, temp, humidity + system info (uptime, WiFi signal, IP) |
| `/report` | Full JSON data dump (all sensor readings + internal state) |
| `/thresholds` | Show active alert thresholds for CO₂, AQI, temp, humidity |
| `/reboot` | Reboot device (rate limited: once per 60 seconds) |
| `/help` | Command list |

### Automatic Alerts

Notifications fire automatically when readings exceed thresholds:

- 🔴 CO₂ > 1500 ppm
- 🔴 AQI ≥ 3 (moderate/poor air quality)
- 🌡️ Temperature > 30°C
- 💧 Humidity > 75%
- **Cooldown:** Each alert type repeats max once per 5 minutes

Thresholds are configured in web UI → **⚙️ Settings** → **Alerts** tab.

---

## 🔄 Firmware Updates (OTA)

After initial flashing, future updates are done through the main web interface:

1. Edit and recompile the project in Arduino IDE
2. **Sketch → Export Compiled Binary** → saves `.bin` file
3. Open **`http://<device-ip>`** → **⚙️ Settings** → **System** tab
4. Select the `.bin` file → **Upload & Flash**
5. Device automatically reboots with new firmware

---

## 🌳 Project Structure

```
AirMonitorPRO/
├── AirMonitorPRO.ino       Main sketch — setup(), loop(), history engine
├── config.h                Central configuration: pins, timings, NTP defaults
├── data_types.h            SensorData, SystemStatus, HistoryPoint, RingBuffer<>
│
├── sensors.cpp/.h          ENS160 + AHT21 driver, baseline calibration
├── wifi_manager.cpp/.h     WiFi STA/AP mode, reconnect logic, hostname management
├── web_server_module.cpp   HTTP routes, WebSocket, REST API handlers
├── web_ui.h                Embedded single-file web app (HTML/CSS/JS/Dashboard)
├── web_ui.html             Alternative: separate HTML asset
│
├── ota_module.cpp/.h       OTA drag-and-drop with version confirmation modal
├── mqtt_module.cpp/.h      MQTT client wrapper, topic pre-building, pub/sub logic
├── telegram_module.cpp/.h  Telegram Bot API (TLS via WiFiClientSecure, FreeRTOS task)
│
├── rtc_module.cpp/.h       DS3231 driver + NTP client (server/TZ configurable)
├── sd_logger.cpp/.h        CSV logging per date, binary history persistence, export
├── oled_display.cpp/.h     SSD1306 I²C driver, two-page display rotation
├── runtime_settings.cpp/.h NVS-backed settings (intervals, thresholds, credentials)
│
├── ap_setup.h              AP mode configuration (SSID, IP, DNS)
├── ap_setup.html           AP web UI (alternative: separate file)
├── history_engine.h        Template-based ring buffer implementation (24h/7d/30d)
│
└── tools/
    ├── build_web_ui.bat    Minify HTML/CSS/JS for embed (optional)
    └── build_web_ui.py     Python version of minify script
```

---

## 💾 Web API Reference

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/` | Main dashboard (served from SPIFFS) |
| `GET` | `/api/history?range=24h\|7d\|30d` | History data as JSON |
| `GET` | `/api/export?date=YYYY-MM-DD` | Download CSV from SD card |
| `GET` | `/api/export` | List available log dates |
| `GET` | `/api/thresholds` | Get alert thresholds (CO₂, AQI, temp, humidity) |
| `POST` | `/api/thresholds` | Update alert thresholds |
| `POST` | `/api/settings` | Save device settings (WiFi, MQTT, Telegram, NTP, etc.) |
| `GET` | `/api/scan` | Scan WiFi networks (JSON array) |
| `GET` | `/api/tg_test` | Send Telegram test message |
| `POST` | `/api/calibrate` | Reset ENS160 baseline |
| `POST` | `/api/reboot` | Reboot device |
| `POST` | `/api/reset` | Factory reset (clear NVS) → reboot |
| `WS` | `ws://<ip>:81/` | WebSocket: real-time data + bidirectional settings |

### WebSocket Message Format

**Client → Device (request)**
```json
{"cmd": "get_settings"}
```

**Device → Client (broadcast every 2 seconds)**
```json
{
  "type": "data",
  "temp": 22.4,
  "hum": 48.1,
  "co2": 812,
  "tvoc": 45,
  "aqi": 2,
  "rssi": -58,
  "uptime": 3600,
  "ip": "192.168.1.42",
  "time_short": "14:32:10",
  "date": "2026-04-12",
  "ver": "1.0.0",
  "mqtt": true,
  "tg": true,
  "sd": true,
  "sd_used_mb": 12,
  "sd_total_mb": 3600,
  "sd_pct": 0.3,
  "rtc": true,
  "ens": true,
  "aht": true,
  "warmup": false,
  "warmup_pct": 100,
  "hist24_rev": 5,
  "hist7_rev": 1,
  "hist30_rev": 0
}
```

---

## 🔌 MQTT Topic Reference

Default prefix: `airmonitor` (customizable via web UI)

| Topic | Payload | Direction | Frequency |
|---|---|---|---|
| `airmonitor/temp` | `22.4` (float) | Publish | 30s |
| `airmonitor/hum` | `48.1` (float) | Publish | 30s |
| `airmonitor/co2` | `812` (int) | Publish | 30s |
| `airmonitor/tvoc` | `45` (int) | Publish | 30s |
| `airmonitor/aqi` | `2` (1–5) | Publish | 30s |
| `airmonitor/rssi` | `-58` (dBm) | Publish | 30s |
| `airmonitor/json` | Full sensor object | Publish | 30s |
| `airmonitor/status` | `online` | Publish (retained) | On connect |
| `airmonitor/cmd` | `reboot` / `reset` | Subscribe | On demand |

---

## 🔬 Sensor Specifications

| Sensor | Parameter | Range | Accuracy |
|---|---|---|---|
| **ENS160** | eCO₂ | 400–8000 ppm | ±10% |
| | TVOC | 0–32000 ppb | — |
| | AQI | 1–5 | Calculated |
| **AHT21** | Temperature | -40 to +80°C | ±0.3°C |
| | Humidity | 0–100% RH | ±1.3% RH |
| **DS3231** | Time Accuracy | — | ±2 ppm |

---

## 📝 Configuration Deep Dive

### Network Defaults (compile-time)

```cpp
#define SSID_AP "AirMonitor_SETUP"        // AP mode SSID
#define AP_IP 192.168.4.1                 // AP web UI IP
#define NTP_SERVER1 "pool.ntp.org"        // Primary NTP source
#define NTP_SERVER2 "time.google.com"     // Fallback NTP source
#define TZ_OFFSET_SEC 10800               // UTC offset in seconds (UTC+3)
```

### Alert Thresholds (runtime, NVS-persisted)

Default values (changeable via web UI):

```
CO₂:           1500 ppm
AQI:           3 (moderate)
Temperature:   30°C
Humidity:      75%
Alert cooldown: 5 minutes per type
```

### Logging Configuration (runtime)

```
Sensor read interval:     10 seconds
WebSocket broadcast:      2 seconds
MQTT publish interval:    30 seconds
History flush to SD:      5 minutes
ENS160 warm-up duration:  3 minutes
```

---

## 🚨 Troubleshooting

| Issue | Solution |
|---|---|
| **COM port not visible** | 1. Check USB cable (data-capable)<br/>2. Try different USB port<br/>3. Install CP210x or CH340 driver<br/>4. Restart Arduino IDE |
| **Upload fails: "Failed to connect"** | 1. Hold BOOT button<br/>2. Click Upload<br/>3. Wait for "Connecting..."<br/>4. Release BOOT<br/>5. Some v4 boards auto-bootload |
| **Compiler: library not found** | 1. Tools → Manage Libraries<br/>2. Install SparkFun versions<br/>3. Restart IDE<br/>4. Recompile |
| **OLED shows nothing** | 1. Check I²C address (0x3C or 0x3D)<br/>2. Verify SDA/SCL pull-ups<br/>3. Use I²C scanner sketch |
| **ENS160 not detected** | 1. Check ADDR pin (HIGH=0x53, LOW=0x52)<br/>2. Verify I²C address in config.h<br/>3. ENS160 needs 3-min warm-up |
| **AHT21 not detected** | 1. Fixed address 0x38<br/>2. Check power and connections<br/>3. Use I²C scanner sketch |
| **Telegram bot silent** | 1. Send `/start` to bot first<br/>2. Verify token and chat_id<br/>3. Try "Test Send" button<br/>4. Check firewall/WiFi HTTPS outbound<br/>5. View `[TG]` logs in Serial Monitor |
| **JSON parse NoMemory error** | Fixed in v1.0.0 (buffer 256→4096). Verify firmware is current. |
| **Device reboots constantly** | 1. Check `/reboot` cooldown (60s)<br/>2. Monitor Serial for Exception<br/>3. Verify 3.3V power stable<br/>4. Check RAM isn't exhausted |
| **WiFi drops after Telegram** | Fixed in v1.0.0 (WiFiClientSecure local scope). Update firmware. |

---

## 📊 Performance & Optimization

- **Memory**: History buffers statically allocated. JSON uses `String::reserve()` to minimize realloc.
- **Heap**: `WiFiClientSecure` local scope, mbedTLS buffers fully released after API calls.
- **MQTT**: Topic strings pre-built; values via `snprintf` (no temp String objects).
- **Templates**: History engine unified (no code duplication).
- **Date/Time**: Cached per loop, reused across logging, WebSocket, OLED.
- **Logging**: ~90% spam reduction, critical events logged.

---

## 📁 SD Card Structure

```
/logs/
  ├── 2026-04-10.csv
  ├── 2026-04-11.csv
  └── ...
/stats/
  ├── history24.bin
  ├── history7d.bin
  └── history30d.bin
```

### CSV Format

```
datetime,temp_c,hum_pct,co2_ppm,tvoc_ppb,aqi
2026-04-10 14:00:15,23.5,45.2,850,120,2
2026-04-10 14:10:22,23.8,46.0,865,135,2
```

---

## 🛠 For Developers

### Build with PlatformIO (Alternative to Arduino IDE)

```bash
# Install PlatformIO CLI
pip install platformio

# Build
platformio run

# Upload
platformio run --target upload

# Serial monitor
platformio device monitor --baud 115200
```

---

## 📜 License

MIT — see LICENSE file.
# 🌿 Air Monitor PRO

> ESP32-based indoor
