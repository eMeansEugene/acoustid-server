@page testing Тестирование

[TOC]

Тесты организованы на трёх уровнях — юнит, интеграционные и end-to-end — и
собраны в один бинарник на модуль через Google Test (подключается через
`FetchContent`). Все тесты детерминированы: используются синтетические
сигналы (суммы синусов) и in-memory SQLite (`":memory:"`), реальные аудиофайлы
и сеть не требуются.

@section testing_run Как запускать

```bash
cmake -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Запустить один модуль или один тест:

```bash
ctest --test-dir build -R VotingEngineTest --output-on-failure
ctest --test-dir build -R VotingEngineTest.ClearWinnerIdentifiedCorrectly
```

Или напрямую через бинарник gtest (полезно для `--gtest_filter` с шаблоном
и для отладчика):

```bash
./build/aid_core_tests --gtest_filter='PeakExtractorTest.*'
```

@section testing_unit Юнит-тесты

| Файл | Компонент | Что проверяется |
|---|---|---|
| `tests/core/fft_engine_test.cpp` | `FftEngine` | Валидация конфига; синус на частоте F даёт пик на ожидаемом бине; нулевой сигнал даёт минимальную мощность; число фреймов соответствует формуле перекрытия. |
| `tests/core/peak_extractor_test.cpp` | `PeakExtractor` | Спектр с известными максимумами даёт правильные пики; порог (медиана + `offset_db_`) отсекает слабые точки; контроль плотности per-band/per-zone работает корректно; краевые случаи (слишком мало фреймов/бинов). |
| `tests/core/hash_generator_test.cpp` | `HashGenerator` | Корректность упаковки трёх чисел в `uint32_t`; `PackHash`/`UnpackHash` — взаимно обратные операции; `freq_bin_limit_` отсекает верхние частоты. |
| `tests/core/voting_engine_test.cpp` | `VotingEngine` | Известный набор `HashMatch` даёт правильный трек, смещение и score; двухуровневое голосование — разные Δ одного трека не конкурируют друг с другом (см. @ref arch_algo_voting); результат ниже `min_votes_`/`min_score_ratio_` возвращает `nullopt`. |
| `tests/audio/audio_decoder_test.cpp` | `AudioDecoder` | WAV и MP3 декодируются в корректные float-сэмплы; формат определяется по содержимому, а не по расширению; многоканальный вход сводится к одному каналу. |
| `tests/storage/sqlite_repository_test.cpp` | `SQLiteRepository` | Запись и чтение треков/fingerprints; `FindMatches` находит совпадения по хэшу; `DeleteTrack` каскадно удаляет fingerprints; конкурентный доступ не портит данные. |
| `tests/server/server_test.cpp` | `TaskQueue`, `TaskRegistry` | FIFO-порядок и потокобезопасность очереди, включая `Stop()` во время ожидающего `Pop()`; переходы статусов в реестре (`PENDING` → `PROCESSING` → `DONE`/`ERROR`). |

@section testing_integration Интеграционные тесты

`tests/domain/domain_integration_test.cpp` (класс `DomainIntegrationTest`)
собирает реальный DSP-пайплайн (`AudioFingerprintEngine`) с in-memory
`SQLiteRepository` — без моков, но и без файлов на диске:

| Сценарий | Что проверяется |
|---|---|
| `IndexTrackStoresMetadataAndFingerprints` | Трек и его fingerprints сохраняются в БД с корректными метаданными. |
| `MatchFragmentFindsCorrectTrack` | Фрагмент, вырезанный из середины проиндексированного трека, определяется корректно (правильный `track_id`, смещение сопоставимо с ожидаемым). |
| `MatchUnknownFragmentReturnsNullopt` | Фрагмент, не связанный ни с одним треком в базе, не даёт ложного совпадения. |
| `MatchEmptyDatabaseReturnsNullopt` | Запрос к пустой базе корректно возвращает "нет совпадения", а не падает. |
| `IndexMultipleTracksAndMatchEach` | Несколько треков в базе не мешают друг другу — каждый фрагмент находит свой трек. |

@section testing_e2e End-to-end

Тесты из @ref testing_integration фактически прогоняют пайплайн целиком —
от синтетического сигнала до результата голосования — без реальных
аудиофайлов, но реализуя тот же сценарий, что описан как e2e: сгенерировать
сигнал → проиндексировать → взять фрагмент из середины → прогнать через
полный пайплайн → проверить трек, score и смещение.

Проверку на **реальных** записях (например, с микрофона) тесты не покрывают
— это делается вручную через запущенный сервер:

```bash
./build/acoustid_server --config config.json &
curl -s -X POST http://localhost:8080/match -F "file=@recording.wav" | python3 -m json.tool
```

Если в конфиге включён `debug_save_audio` (см. @ref cfg_server), каждый
входящий на `/match` файл сохраняется в `debug_audio_dir` — удобно
накапливать реальные записи для регрессионного прогона вручную.

@section testing_ci CI

Каждый push должен запускать pipeline: сборка → тесты (см. NFR-4 в
требованиях проекта). Локально тот же путь воспроизводится командами из
@ref testing_run.
