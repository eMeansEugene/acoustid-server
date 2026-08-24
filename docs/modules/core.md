@page module_core Модуль core

[TOC]

@section core_purpose Назначение

`core/` — вычислительное ядро: DSP-пайплайн, превращающий поток float-сэмплов
в набор компактных хэшей (fingerprints), и алгоритм голосования, извлекающий
из совпавших хэшей трек и позицию совпадения. Модуль ничего не знает про
аудиоформаты, HTTP или БД — принимает и отдаёт только простые структуры
данных, поэтому легко тестируется в изоляции синтетическими сигналами.

@section core_classes Классы

| Класс / структура | Роль |
|---|---|
| `FftEngine` | Строит спектрограмму из сэмплов (окно Ханна + pocketfft r2c) |
| `Spectrogram` | Матрица (фреймы × бины) логарифмической мощности |
| `PeakExtractor` | Извлекает constellation map (локальные максимумы) из спектрограммы |
| `HashGenerator` | Кодирует пары пиков в 32-битные хэши |
| `VotingEngine` | Голосование по совпавшим хэшам: находит трек и смещение |
| `AudioFingerprintEngine` | Фасад: сэмплы → спектрограмма → пики → fingerprints одним вызовом |

@section core_dataflow Data flow

```mermaid
flowchart LR
    S["vector&lt;float&gt; samples"] --> FE["FftEngine"]
    FE --> SP["Spectrogram"]
    SP --> PE["PeakExtractor"]
    PE --> PK["vector&lt;Peak&gt;"]
    PK --> HG["HashGenerator"]
    HG --> FP["vector&lt;Fingerprint&gt;"]
```

Все четыре шага объединены в `AudioFingerprintEngine::Process()`, которая
возвращает `FingerprintResult{spectrogram, peaks, fingerprints}` — не только
финальные хэши, но и промежуточные данные, нужные HTTP-слою для
визуализации.

Отдельно, при распознавании, `VotingEngine::Vote()` принимает список
`HashMatch` (результат join хэшей фрагмента с хэшами из БД) и возвращает
`optional<MatchResult>` — см. @ref arch_algo_voting в @ref architecture.

@section core_usage Пример использования

```cpp
#include "core/audio_fingerprint_engine.h"

aid::core::AudioFingerprintEngine engine(
    aid::core::FftEngineConfig{.frame_size_ = 2048, .hop_size_ = 1024},
    aid::core::PeakExtractorConfig{.offset_db_ = 6.0F, .zone_frames_ = 43},
    aid::core::HashGeneratorConfig{.max_targets_per_anchor_ = 3});

std::vector<float> samples = /* декодированные моно-сэмплы */;
aid::core::FingerprintResult result = engine.Process(samples);
// result.fingerprints — то, что сохраняется в БД или ищется в ней.
```

```cpp
#include "core/voting_engine.h"

aid::core::VotingEngine voter({.min_votes_ = 10, .min_score_ratio_ = 2.0});
std::vector<aid::core::HashMatch> matches = /* join фрагмента с БД */;
if (auto result = voter.Vote(matches)) {
    // result->track_id_, result->offset_frames_, result->score_
}
```

@section core_deps Зависимости

- Не зависит от других модулей проекта.
- Внешние библиотеки: pocketfft (FFT), стандартная библиотека C++20.
- От `core` зависят `domain` (оркестрация через `AudioFingerprintEngine` и
  `VotingEngine`) и `app_config` (хранит `FftEngineConfig`,
  `PeakExtractorConfig`, `HashGeneratorConfig`, `VotingEngineConfig`).
