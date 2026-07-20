var index =
[
    [ "Audio Fingerprinting Server", "index.html#autotoc_md1", null ],
    [ "Описание проекта", "index.html#autotoc_md3", [
      [ "Стек технологий", "index.html#autotoc_md4", null ]
    ] ],
    [ "Требования", "index.html#autotoc_md6", [
      [ "Функциональные требования", "index.html#autotoc_md7", [
        [ "FR-1 — Индексирование аудио", "index.html#autotoc_md8", null ],
        [ "FR-2 — Матчинг фрагмента", "index.html#autotoc_md9", null ],
        [ "FR-3 — REST API (публичный)", "index.html#autotoc_md10", null ],
        [ "FR-4 — REST API (административный)", "index.html#autotoc_md11", null ],
        [ "FR-5 — Визуализация (опционально, реализуется последней)", "index.html#autotoc_md12", null ]
      ] ],
      [ "Нефункциональные требования", "index.html#autotoc_md13", [
        [ "NFR-1 — Производительность", "index.html#autotoc_md14", null ],
        [ "NFR-2 — Надёжность", "index.html#autotoc_md15", null ],
        [ "NFR-3 — Портируемость", "index.html#autotoc_md16", null ],
        [ "NFR-4 — Сопровождаемость", "index.html#autotoc_md17", null ],
        [ "NFR-5 — Наблюдаемость", "index.html#autotoc_md18", null ]
      ] ]
    ] ],
    [ "Архитектура", "index.html#autotoc_md20", [
      [ "Слоистая архитектура", "index.html#autotoc_md21", null ],
      [ "Два входа для индексирования", "index.html#autotoc_md22", null ],
      [ "Асинхронная обработка", "index.html#autotoc_md23", null ],
      [ "Синхронизация доступа к БД", "index.html#autotoc_md24", null ]
    ] ],
    [ "Поток данных", "index.html#autotoc_md26", [
      [ "Индексирование трека", "index.html#autotoc_md27", null ],
      [ "Матчинг фрагмента", "index.html#autotoc_md28", null ]
    ] ],
    [ "Алгоритмы", "index.html#autotoc_md30", [
      [ "Спектрограмма", "index.html#autotoc_md31", null ],
      [ "Дискретное преобразование Фурье и FFT", "index.html#autotoc_md32", null ],
      [ "Constellation map", "index.html#autotoc_md33", null ],
      [ "Хэши", "index.html#autotoc_md34", null ],
      [ "Голосование", "index.html#autotoc_md35", null ],
      [ "Confidence score", "index.html#autotoc_md36", null ]
    ] ],
    [ "База данных", "index.html#autotoc_md38", [
      [ "Схема", "index.html#autotoc_md39", null ],
      [ "Индексы", "index.html#autotoc_md40", null ],
      [ "Конкурентный доступ", "index.html#autotoc_md41", null ],
      [ "Оценка объёма", "index.html#autotoc_md42", null ]
    ] ],
    [ "REST API", "index.html#autotoc_md44", [
      [ "Публичные эндпоинты", "index.html#autotoc_md45", [
        [ "POST /match", "index.html#autotoc_md46", null ],
        [ "GET /tasks/{id}", "index.html#autotoc_md47", null ]
      ] ],
      [ "Административные эндпоинты", "index.html#autotoc_md48", [
        [ "POST /admin/index", "index.html#autotoc_md49", null ]
      ] ],
      [ "Коды ошибок", "index.html#autotoc_md50", null ]
    ] ],
    [ "Тестирование", "index.html#autotoc_md52", [
      [ "Юнит тесты", "index.html#autotoc_md53", null ],
      [ "Интеграционные тесты", "index.html#autotoc_md54", null ],
      [ "End-to-end тест", "index.html#autotoc_md55", null ]
    ] ],
    [ "Архитектурные решения", "index.html#autotoc_md57", [
      [ "SQLite вместо PostgreSQL", "index.html#autotoc_md58", null ],
      [ "pocketfft вместо FFTW", "index.html#autotoc_md59", null ],
      [ "Crow вместо Boost.Beast", "index.html#autotoc_md60", null ],
      [ "32-битный хэш", "index.html#autotoc_md61", null ],
      [ "Асинхронный матчинг", "index.html#autotoc_md62", null ],
      [ "Относительный порог отбора пиков", "index.html#autotoc_md63", null ]
    ] ],
    [ "Возможные улучшения", "index.html#autotoc_md65", [
      [ "Без изменения архитектуры", "index.html#autotoc_md66", null ],
      [ "Расширение архитектуры", "index.html#autotoc_md67", null ],
      [ "Смена стека", "index.html#autotoc_md68", null ]
    ] ],
    [ "Критерии завершения", "index.html#autotoc_md70", [
      [ "Функциональность", "index.html#autotoc_md71", null ],
      [ "База данных", "index.html#autotoc_md72", null ],
      [ "Сборка и CI", "index.html#autotoc_md73", null ],
      [ "Тестирование", "index.html#autotoc_md74", null ],
      [ "Документация", "index.html#autotoc_md75", null ]
    ] ]
];