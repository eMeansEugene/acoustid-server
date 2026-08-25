var index =
[
    [ "Обзор", "index.html#intro", null ],
    [ "Быстрый старт", "index.html#mainpage_quickstart", null ],
    [ "Модули", "index.html#modules", null ],
    [ "Архитектура", "index.html#mainpage_architecture", null ],
    [ "Руководства", "index.html#guides", null ],
    [ "Архитектура", "architecture.html", [
      [ "Слоистая архитектура", "architecture.html#arch_layers", null ],
      [ "Два входа для индексирования", "architecture.html#arch_indexing_entry", null ],
      [ "Асинхронная обработка распознавания", "architecture.html#arch_async", null ],
      [ "Синхронизация доступа к БД", "architecture.html#arch_db_sync", null ],
      [ "Поток данных: индексирование трека", "architecture.html#arch_dataflow_index", null ],
      [ "Поток данных: распознавание фрагмента", "architecture.html#arch_dataflow_match", null ],
      [ "Алгоритмы", "architecture.html#arch_algorithms", [
        [ "Спектрограмма", "architecture.html#arch_algo_spectrogram", null ],
        [ "Constellation map", "architecture.html#arch_algo_constellation", null ],
        [ "Хэши", "architecture.html#arch_algo_hashes", null ],
        [ "Голосование (two-level: track → delta)", "architecture.html#arch_algo_voting", null ],
        [ "Частотные полосы", "architecture.html#arch_freq_bands", null ]
      ] ],
      [ "Архитектурные решения", "architecture.html#arch_decisions", [
        [ "SQLite вместо PostgreSQL", "architecture.html#arch_dec_sqlite", null ],
        [ "pocketfft вместо FFTW", "architecture.html#arch_dec_pocketfft", null ],
        [ "Crow вместо Boost.Beast", "architecture.html#arch_dec_crow", null ],
        [ "32-битный хэш", "architecture.html#arch_dec_hash32", null ],
        [ "Асинхронное распознавание", "architecture.html#arch_dec_async", null ],
        [ "Относительный порог отбора пиков", "architecture.html#arch_dec_threshold", null ]
      ] ]
    ] ],
    [ "Быстрый старт", "quickstart.html", [
      [ "Сборка", "quickstart.html#qs_build", null ],
      [ "Конфигурация", "quickstart.html#qs_config", null ],
      [ "Индексирование треков", "quickstart.html#qs_index", null ],
      [ "Запуск сервера", "quickstart.html#qs_run", null ],
      [ "Первый запрос на распознавание", "quickstart.html#qs_first_request", null ],
      [ "Дальше", "quickstart.html#qs_next", null ]
    ] ],
    [ "Модуль core", "module_core.html", [
      [ "Назначение", "module_core.html#core_purpose", null ],
      [ "Классы", "module_core.html#core_classes", null ],
      [ "Data flow", "module_core.html#core_dataflow", null ],
      [ "Пример использования", "module_core.html#core_usage", null ],
      [ "Зависимости", "module_core.html#core_deps", null ]
    ] ],
    [ "Модуль audio", "module_audio.html", [
      [ "Назначение", "module_audio.html#audio_purpose", null ],
      [ "Классы", "module_audio.html#audio_classes", null ],
      [ "Форматы и конвертация", "module_audio.html#audio_formats", null ],
      [ "Data flow", "module_audio.html#audio_dataflow", null ],
      [ "Пример использования", "module_audio.html#audio_usage", null ],
      [ "Зависимости", "module_audio.html#audio_deps", null ]
    ] ],
    [ "Модуль domain", "module_domain.html", [
      [ "Назначение", "module_domain.html#domain_purpose", null ],
      [ "Классы", "module_domain.html#domain_classes", null ],
      [ "Data flow", "module_domain.html#domain_dataflow", null ],
      [ "Пример использования", "module_domain.html#domain_usage", null ],
      [ "Зависимости", "module_domain.html#domain_deps", null ]
    ] ],
    [ "Модуль storage", "module_storage.html", [
      [ "Назначение", "module_storage.html#storage_purpose", null ],
      [ "Классы", "module_storage.html#storage_classes", null ],
      [ "Схема БД", "module_storage.html#storage_schema", null ],
      [ "WAL и конкурентность", "module_storage.html#storage_concurrency", null ],
      [ "Пример использования", "module_storage.html#storage_usage", null ],
      [ "Зависимости", "module_storage.html#storage_deps", null ]
    ] ],
    [ "Модуль server", "module_server.html", [
      [ "Назначение", "module_server.html#server_purpose", null ],
      [ "Классы", "module_server.html#server_classes", null ],
      [ "Data flow", "module_server.html#server_dataflow", null ],
      [ "Пример использования", "module_server.html#server_usage", null ],
      [ "Зависимости", "module_server.html#server_deps", null ]
    ] ],
    [ "Веб-клиент", "web_client.html", [
      [ "Обзор", "web_client.html#wc_overview", null ],
      [ "Интерфейс", "web_client.html#wc_ui", null ],
      [ "Запись через MediaRecorder и конвертация в WAV", "web_client.html#wc_recording", null ],
      [ "Визуализация спектра", "web_client.html#wc_viz", null ],
      [ "Взаимодействие с REST API", "web_client.html#wc_api", null ],
      [ "Как запустить", "web_client.html#wc_run", null ]
    ] ],
    [ "Конфигурация", "configuration.html", [
      [ "server", "configuration.html#cfg_server", null ],
      [ "database", "configuration.html#cfg_database", null ],
      [ "fft", "configuration.html#cfg_fft", null ],
      [ "peak_extractor", "configuration.html#cfg_peak", null ],
      [ "hash_generator", "configuration.html#cfg_hash", null ],
      [ "voting", "configuration.html#cfg_voting", null ],
      [ "Как менять параметры осмысленно", "configuration.html#cfg_tuning", null ]
    ] ],
    [ "Тестирование", "testing.html", [
      [ "Как запускать", "testing.html#testing_run", null ],
      [ "Юнит-тесты", "testing.html#testing_unit", null ],
      [ "Интеграционные тесты", "testing.html#testing_integration", null ],
      [ "End-to-end", "testing.html#testing_e2e", null ],
      [ "CI", "testing.html#testing_ci", null ]
    ] ]
];