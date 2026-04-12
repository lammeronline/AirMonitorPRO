# 🔥 Как подготовить среду и прошить Air Monitor PRO

---

## 📋 Что нужно

| Что | Где взять |
|---|---|
| Arduino IDE 2.x | https://www.arduino.cc/en/software |
| ESP32 board package | Boards Manager в IDE |
| USB-кабель с данными (не только зарядка!) | — |
| Windows 10/11 | Основная целевая среда сборки и прошивки |
| Драйвер CP2102/CP210x или CH340 | Зависит от вашей платы ESP32 |

> Эта инструкция описывает **основной путь для Windows**. Анализировать и редактировать проект можно на Ubuntu, но параметры сборки и шаги загрузки ниже рассчитаны именно на Windows.

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
| RTClib | `RTClib` | Adafruit |
| WebSockets | `WebSockets` | Markus Sattler |
| ArduinoJson | `ArduinoJson` | Benoit Blanchon |
| PubSubClient | `PubSubClient` | Nick O'Leary |
| SparkFun ENS160 | `SparkFun ENS160` | SparkFun |
| SparkFun Qwiic Humidity AHT20 | `SparkFun Qwiic Humidity AHT20` | SparkFun |

> **Важно:** При установке Adafruit SSD1306 IDE предложит "Install all dependencies" — нажмите **Yes**.
>
> Этот список взят из реального кода проекта. Если ориентироваться только на старую документацию, можно поставить несовместимые библиотеки датчиков.

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

// При необходимости можно временно включить сброс NVS
#define FORCE_FACTORY_RESET 0
```

Пороговые значения Telegram и MQTT не задаются в `config.h` на постоянной основе:
они сохраняются в NVS через веб-интерфейс после первого подключения устройства к сети.

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
| **Port** | COM3 или другой обнаруженный `COMx` |

---

## ШАГ 5 — Подключение ESP32

1. Подключите ESP32 к USB
2. Убедитесь, что в `Tools → Port` появился нужный `COM`-порт
3. Нажмите кнопку **BOOT** на плате и держите
4. В Arduino IDE нажмите **Upload** (→)
5. Когда в консоли появится `Connecting....` — отпустите BOOT
6. Дождитесь `Hard resetting via RTS pin...` — прошивка завершена!

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
[Sensors] ENS160 OK
[OLED] SSD1306 OK
[RTC] DS3231 OK
[WiFi] Starting AP: AirMonitor_SETUP
[Web] HTTP:80 WS:81
[Main] Setup complete
```

Если некоторых модулей физически нет на плате или они не подключены, сообщения будут указывать на конкретный отказ (`FAIL` / `not found`). Для проверки среды сборки это допустимо: главное, чтобы прошивка загрузилась и устройство стартовало.

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
| COM-порт не появляется | Проверьте data-кабель, затем установите драйвер `CP210x` или `CH340` |
| `Failed to connect to ESP32` | Держите кнопку BOOT при прошивке |
| Компилятор ругается на библиотеку | Проверьте версию (ArduinoJson нужна ≥6.x) |
| Не находятся ENS160/AHT20 библиотеки | Убедитесь, что установлены именно `SparkFun ENS160` и `SparkFun Qwiic Humidity AHT20` |
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
