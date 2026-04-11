# 🌿 Air Monitor PRO — ESP32

Полноценная IoT-станция мониторинга воздуха.

---

## 📦 Необходимые библиотеки

Установить через **Arduino Library Manager**:

| Библиотека | Автор |
|---|---|
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |
| Adafruit AHTX0 | Adafruit |
| DFRobot_ENS160 | DFRobot |
| RTClib | Adafruit |
| WebSockets | Markus Sattler |
| ArduinoJson (≥6.x) | Benoit Blanchon |
| SD (встроенная) | Arduino |

---

## 🔌 Схема подключения

### I²C шина (SDA=21, SCL=22)
```
ESP32        SSD1306      ENS160       AHT21        DS3231
3.3V  ───── VCC          VCC          VCC           VCC
GND   ───── GND          GND          GND           GND
GPIO21 ──── SDA          SDA          SDA           SDA
GPIO22 ──── SCL          SCL          SCL           SCL
                         ADDR ─── 3.3V (→ 0x53)
```

### SPI (SD карта)
```
ESP32        SD Module
GPIO18 ───── CLK
GPIO19 ───── MISO
GPIO23 ───── MOSI
GPIO5  ───── CS
3.3V  ───── VCC
GND   ───── GND
```

---

## ⚙️ Первый запуск

1. Прошейте ESP32
2. Устройство создаст точку доступа **AirMonitor_SETUP**
3. Подключитесь к ней с телефона
4. Откройте браузер → `http://192.168.4.1`
5. Выберите сеть, введите пароль → **Подключить**
6. ESP32 перезагрузится и подключится к вашей сети
7. Найдите IP адрес на OLED (страница 2) или в роутере
8. Откройте `http://<IP>` — готово!

---

## 🌐 Веб-интерфейс

| URL | Описание |
|---|---|
| `http://<IP>/` | Главный дашборд |
| `http://<IP>/update` | OTA прошивка |
| `http://<IP>/api/scan` | JSON список WiFi |
| `http://<IP>/api/settings` POST | Сохранить настройки |
| `http://<IP>/api/reboot` POST | Перезагрузка |
| `http://<IP>/api/reset` POST | Сброс настроек |

WebSocket: `ws://<IP>:81/` — real-time данные каждые 2 сек.

---

## 💾 Логи на SD

Структура: `/logs/YYYY-MM-DD.csv`

```
datetime,temp_c,hum_pct,co2_ppm,tvoc_ppb,aqi
2026-04-10 14:00,23.5,45.2,850,120,2
```

---

## 🔧 Настройки компилятора (Arduino IDE)

- Board: **ESP32 Dev Module**
- Flash Size: 4MB
- Partition Scheme: **Default 4MB with spiffs**
- Upload Speed: 921600
- CPU Frequency: 240MHz

---

## 📐 Архитектура

```
AirMonitorPRO.ino      ← Главный скетч (оркестратор)
├── config.h            ← Все константы, пины, макросы
├── data_types.h        ← SensorData, SystemStatus, RingBuffer
├── wifi_manager        ← AP/STA + NVS + переподключение
├── web_server_module   ← HTTP API + WebSocket push
├── web_ui.h            ← Встроенный HTML дашборд (PROGMEM)
├── sensors             ← ENS160 + AHT21 (неблокирующий)
├── oled_display        ← SSD1306, 2 страницы, авторотация
├── sd_logger           ← CSV лог каждую минуту
├── rtc_module          ← DS3231 + NTP синхронизация
└── ota_module          ← /update страница + flash
```

---

## 🚀 Дальнейшее развитие

- `[ ]` MQTT публикация (PubSubClient)
- `[ ]` Telegram уведомления (превышение порогов)
- `[ ]` Home Assistant (MQTT Discovery)
- `[ ]` Grafana / InfluxDB экспорт
- `[ ]` Режим LOW POWER (light sleep между чтениями)
- `[ ]` REST API `/api/history` — отдача буфера истории в JSON
