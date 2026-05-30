# Beer Race

Веб-приложение для игры Beer Race с подключением к ESP32 по HTTP API.

## Стек

- React + Vite
- JavaScript
- CSS (без UI-библиотек)

## Запуск

```bash
npm install
npm run dev
```

Сборка для продакшена:

```bash
npm run build
npm run preview
```

## Настройка ESP32

Адрес устройства задаётся в `src/api.js`:

```js
export const API_URL = 'http://192.168.1.123';
```

## API

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/weight` | Текущий вес |
| GET | `/state` | Состояние игры (0–3) |
| GET | `/leaderboard` | Таблица лидеров |
| POST | `/api/start` | Старт игры |
| POST | `/api/cup` | Стакан поставлен |
| POST | `/api/drink` | Начать пить |
| POST | `/api/cancel` | Отмена |

Данные обновляются каждые 200 мс.

## Состояния игры

| Код | Название |
|-----|----------|
| 0 | Ready |
| 1 | Put Cup |
| 2 | Ready To Start |
| 3 | Drinking |
