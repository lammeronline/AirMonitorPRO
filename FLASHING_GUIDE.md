# 🔥 Как прошить Air Monitor PRO

---

## 📋 Что нужно

| Что | Где взять |
|---|---|
| Arduino IDE 2.x | https://www.arduino.cc/en/software |
| ESP32 board package | Boards Manager в IDE |
| USB-кабель с данными (не только зарядка!) | — |
| Драйвер CP2102 или CH340 | Зависит от вашей платы |

---

## ШАГ 1 — Установка Arduino IDE + ESP32

### 1.1 Добавить ESP32 в Board Manager

`File → Preferences → Additional boards manager URLs`:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### 1.2 Установить пакет

`Tools → Board → Boards Manager` → найти **esp32 by Espressif Systems** → Install

---

## ШАГ 2 — Установка библиотек

`Tools → Manage Libraries` → установить каждую:

| Библиотека | Поиск | Автор |
|---|---|---|
| Adafruit SSD1306 | `SSD1306` | Adafruit |
| Adafruit GFX Library | `Adafruit GFX` | Adafruit |
| Adafruit AHTX0 | `AHTX0` | Adafruit |
| DFRobot_ENS160 | `ENS160 DFRobot` | DFRobot |
| RTClib | `RTClib` | Adafruit |
| WebSockets | `WebSockets` | Markus Sattler |
| ArduinoJson | `ArduinoJson` | Benoit Blanchon |
| PubSubClient | `PubSubClient` | Nick O'Leary |

> **Важно:** При установке Adafruit SSD1306 IDE предложит "Install all dependencies" — нажмите **Yes**.

---

## ШАГ 3 — Настройка проекта

### 3.1 Распакуйте архив

Распакуйте `AirMonitorPRO.zip` в любую папку.  
Важно: **папка должна называться** `AirMonitorPRO` (как .ino файл).

### 3.2 Откройте проект

`File → Open` → выберите `AirMonitorPRO.ino`

### 3.3 (Опционально) Настройки в config.h

```cpp
// Ваш часовой пояс (UTC+3 = Украина/Москва)
#define TZ_OFFSET_SEC 10800

// Пороги алертов (в telegram_module.cpp)
static const uint16_t ALERT_CO2_PPM = 1500;  // ppm
static const uint8_t  ALERT_AQI_LVL = 3;     // 1-5
```

---

## ШАГ 4 — Настройки компилятора

`Tools` меню:

| Параметр | Значение |
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
| **Port** | COM3 (или /dev/ttyUSB0 на Linux/Mac) |

---

## ШАГ 5 — Подключение ESP32

1. Подключите ESP32 к USB
2. Нажмите кнопку **BOOT** на плате и держите
3. В Arduino IDE нажмите **Upload** (→)
4. Когда в консоли появится `Connecting....` — отпустите BOOT
5. Дождитесь `Hard resetting via RTS pin...` — прошивка завершена!

> На некоторых платах (ESP32-DevKit v4) BOOT кнопку держать не нужно — всё автоматически.

---

## ШАГ 6 — Проверка

Откройте `Tools → Serial Monitor`:
- Baudrate: **115200**
- Должны увидеть:

```
╔══════════════════════════════╗
║   Air Monitor PRO  v1.0.0   ║
╚══════════════════════════════╝
[SD] Card OK
[Sensors] AHT21 OK
[Sensors] ENS160 OK (warming up 3 min)
[OLED] SSD1306 OK
[RTC] DS3231 OK
[WiFi] Starting AP: AirMonitor_SETUP
[Web] HTTP port 80, WS port 81
[Main] Setup complete
```

---

## ШАГ 7 — Первичная настройка WiFi

1. С телефона подключитесь к точке доступа **`AirMonitor_SETUP`**
2. Откройте браузер → `http://192.168.4.1`
3. Выберите вашу сеть из списка → введите пароль → **Подключить**
4. ESP32 перезагрузится и подключится к роутеру
5. IP-адрес появится на OLED (страница 2) и в Serial Monitor

---

## ШАГ 8 — Настройка MQTT (если нужен)

1. Откройте `http://<IP>` → ⚙️ → вкладка **MQTT**
2. Включите тумблер
3. Введите хост вашего брокера (например Mosquitto, HiveMQ, MQTT.fx)
4. Нажмите **Сохранить**

Данные публикуются каждые 30 секунд:
```
airmonitor/temp   → 23.5
airmonitor/hum    → 45.2
airmonitor/co2    → 850
airmonitor/tvoc   → 120
airmonitor/aqi    → 2
airmonitor/json   → {"temp":23.5,"hum":45.2,...}
airmonitor/status → online
```

Команды (publish в `airmonitor/cmd`):
```
reboot  → перезагрузка
reset   → сброс настроек
```

---

## ШАГ 9 — Настройка Telegram бота

### 9.1 Создайте бота

1. В Telegram найдите **@BotFather**
2. Напишите `/newbot`
3. Придумайте имя и username
4. Скопируйте токен вида `7123456789:AAFxxxxxxxx`

### 9.2 Получите Chat ID

1. Найдите своего бота в Telegram
2. Напишите ему `/start`
3. Откройте в браузере:
   ```
   https://api.telegram.org/bot<ВАШ_ТОКЕН>/getUpdates
   ```
4. Найдите в ответе `"id":` в секции `"chat"` — это ваш Chat ID

### 9.3 Введите в настройки

1. Откройте `http://<IP>` → ⚙️ → вкладка **Telegram**
2. Включите тумблер
3. Введите Token и Chat ID
4. Нажмите **Тест отправки** — должно прийти сообщение
5. Нажмите **Сохранить**

### 9.4 Команды бота

Напишите боту:
```
/status  — текущие показатели воздуха
/report  — JSON-отчёт
/reboot  — перезагрузить устройство
/help    — список команд
```

Автоматические алерты (сообщение придёт само):
- 🔴 CO₂ > 1500 ppm
- 🔴 AQI ≥ 3 (умеренный/плохой)
- Повтор не чаще раза в 5 минут

---

## 🔄 OTA обновление (без кабеля)

После первой прошивки — все следующие через браузер:

1. Скомпилируйте: `Sketch → Export Compiled Binary` → получите `.bin`
2. Откройте `http://<IP>/update`
3. Выберите `.bin` → **Upload & Flash**
4. Устройство само перезагрузится с новой прошивкой

---

## 🛠 Решение проблем

| Проблема | Решение |
|---|---|
| Port не появляется | Установите драйвер CP2102 или CH340 |
| `Failed to connect to ESP32` | Держите кнопку BOOT при прошивке |
| Компилятор ругается на библиотеку | Проверьте версию (ArduinoJson нужна ≥6.x) |
| OLED не показывает | Проверьте адрес (0x3C или 0x3D), подтяжки SDA/SCL |
| ENS160 не видит датчик | Проверьте ADDR пин (HIGH = 0x53, LOW = 0x52) |
| Telegram не работает | Убедитесь что /start написан боту до настройки |

---

## 📁 Файловая структура SD-карты (результат)

```
/logs/
  2026-04-10.csv
  2026-04-11.csv
  ...
```

Формат CSV:
```
datetime,temp_c,hum_pct,co2_ppm,tvoc_ppb,aqi
2026-04-10 14:00,23.5,45.2,850,120,2
```
