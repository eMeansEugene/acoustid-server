@page quickstart Быстрый старт

[TOC]

@section qs_build Сборка

Требуется CMake 3.20+, компилятор с поддержкой C++20 и системная `sqlite3`
(остальные зависимости — pocketfft, dr_wav, minimp3, nlohmann/json, Crow,
Google Test — подтягиваются автоматически через `FetchContent`).

```bash
cmake -B build
cmake --build build -j$(nproc)
```

Собираются два исполняемых файла: `build/acoustid_server` (HTTP-сервер) и
`build/indexer` (CLI для индексирования).

@section qs_config Конфигурация

Оба исполняемых файла читают JSON-конфиг (по умолчанию `config.json` в
рабочей директории, путь переопределяется через `--config`). Пример — в
`configs/default_config.json`, полное описание всех полей — @ref configuration.
Если файл не найден или не парсится, используются значения по умолчанию
(`AppConfig::Defaults()`).

```bash
cp configs/default_config.json config.json
```

@section qs_index Индексирование треков

Индексирование — административная операция, недоступная обычным
пользователям API. Есть два пути: CLI и HTTP.

**Через CLI:**

```bash
./build/indexer --input track.mp3 --title "Bohemian Rhapsody" --artist "Queen" \
    --config config.json
```

**Через HTTP** (сервер должен быть запущен, см. ниже) — требует заголовок
`X-Api-Key` со значением `admin_api_key` из конфига:

```bash
curl -X POST http://localhost:8080/admin/index \
    -H "X-Api-Key: changeme" \
    -F "file=@track.mp3" \
    -F "title=Bohemian Rhapsody" \
    -F "artist=Queen"
```

Для демонстрационной базы стоит проиндексировать не меньше 10–15
разноплановых треков.

@section qs_run Запуск сервера

Сервер отдаёт `static/index.html` по относительному пути, поэтому его нужно
запускать из корня репозитория (или скопировать `static/` рядом с рабочей
директорией):

```bash
./build/acoustid_server --config config.json
```

По умолчанию сервер слушает `0.0.0.0:8080` (порт настраивается в конфиге).
Веб-клиент доступен на `http://localhost:8080/` — см. @ref web_client.

@section qs_first_request Первый запрос на распознавание

```bash
curl -s -X POST http://localhost:8080/match -F "file=@fragment.wav" | python3 -m json.tool
```

Ответ приходит немедленно и содержит только `task_id` — вычисления идут
асинхронно в `WorkerPool`:

```json
{ "task_id": "a3f1c2d4e5b6" }
```

Опрашиваем результат, пока `status` не станет `done` или `error`:

```bash
curl -s http://localhost:8080/tasks/a3f1c2d4e5b6 | python3 -m json.tool
```

```json
{
  "task_id": "a3f1c2d4e5b6",
  "status": "done",
  "result": {
    "track": { "id": 7, "title": "Bohemian Rhapsody", "artist": "Queen", "duration_sec": 354.5 },
    "offset_frames": 1820,
    "votes": 214,
    "runner_up": 3,
    "score": 71.3
  }
}
```

Если совпадение не найдено — `"result": null`. Полный список эндпоинтов и
формат ответов — в комментариях `HttpServer` (`src/server/http_server.h`) и
в @ref module_server.

@section qs_next Дальше

- @ref configuration — что означает каждый параметр `config.json`.
- @ref testing — как запускать и что проверяет каждый уровень тестов.
- @ref web_client — как устроен веб-клиент, если нужно его доработать.
