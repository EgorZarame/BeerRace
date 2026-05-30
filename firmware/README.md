# Прошивка ESP32-C3

## Как узнать IP адрес ESP32

### Способ 1 — Serial Monitor (самый простой)

1. Подключите ESP32-C3 по USB.
2. Arduino IDE → **Tools → Board** → `ESP32C3 Dev Module`.
3. **Tools → USB CDC On Boot** → `Enabled` (если нет вывода в Serial).
4. **Tools → Port** → выберите порт `usbmodem…` / `COM…`.
5. **Tools → Upload Speed** → `115200`, если прошивка не идёт.
6. Залейте скетч `BeerRace/BeerRace.ino`.
7. Откройте **Serial Monitor**, скорость **115200**.
8. После подключения к Wi‑Fi появится блок:

```
============================================
  IP address:   http://192.168.1.42
  mDNS:         http://beerrace.local
  Скопируйте IP в src/api.js → API_URL
============================================
```

Этот IP вставьте в React:

```js
export const API_URL = 'http://192.168.1.42';
```

### Способ 2 — в браузере

Если ESP32 и телефон/ПК в одной Wi‑Fi сети, откройте:

- `http://beerrace.local/info` (mDNS, не всегда работает на Android)
- или `http://ВАШ_IP/info`

Ответ:

```json
{"ip":"192.168.1.42","hostname":"beerrace","mdns":"beerrace.local","state":0}
```

### Способ 3 — роутер

Зайдите в веб-интерфейс роутера → список устройств → ищите `esp32` / `espressif`.

---

## Настройка перед прошивкой

В `BeerRace.ino` замените:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Пины HX711 по умолчанию: **DT = GPIO 4**, **SCK = GPIO 5**.

---

## Библиотеки

| Библиотека | Установка |
|------------|-----------|
| ESPAsyncWebServer | Library Manager |
| AsyncTCP | Library Manager |
| ArduinoJson 6.x | Library Manager |
| HX711 | Library Manager |

Плата ESP32: через **Boards Manager** → `esp32` by Espressif.

---

## Проверка API

| URL | Ожидание |
|-----|----------|
| `http://IP/weight` | `{"weight":123.4}` |
| `http://IP/state` | `{"state":0}` |
| `http://IP/leaderboard` | `[]` или массив игроков |

После этого запускайте React: `npm run dev -- --host`.
