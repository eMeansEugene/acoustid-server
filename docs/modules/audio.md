@page module_audio Модуль audio

[TOC]

@section audio_purpose Назначение

`audio/` отвечает за единственную задачу — превратить байты аудиофайла
(MP3 или WAV) в моно-сэмплы `float` с известной частотой дискретизации.
Формат вычислительного ядра (`core/`) от аудиоформатов не зависит: модуль
`audio` — единственное место, где нужно что-то менять при добавлении
поддержки, например, FLAC или OGG.

@section audio_classes Классы

| Класс / структура | Роль |
|---|---|
| `AudioDecoder` | Определяет формат по магическим байтам и декодирует MP3/WAV |
| `AudioData` | Результат: моно-сэмплы, частота дискретизации, длительность |

@section audio_formats Форматы и конвертация

Формат определяется **по содержимому**, а не по расширению файла:
`AudioDecoder::DetectFormat()` проверяет сигнатуру `RIFF....WAVE` для WAV и
структуру MP3-фрейма для MP3 (частный случай ошибки — `Format::UNKNOWN`,
приводит к `std::runtime_error`).

- **WAV** декодируется через `dr_wav` (`drwav_read_pcm_frames_f32`) — header-only
  библиотека, поддерживает PCM 16/24/32 бит и float.
- **MP3** декодируется через `minimp3` — header-only декодер без внешних
  зависимостей.

Если исходный файл стерео (или больше каналов), `AudioDecoder::ExtractFirstChannel`
берёт только первый канал — для распознавания достаточно одного, а работа с
несколькими каналами усложнила бы DSP-пайплайн без выигрыша в точности.

@section audio_dataflow Data flow

![diagram](./audio-1.svg)

@section audio_usage Пример использования

```cpp
#include "audio/audio_decoder.h"

aid::audio::AudioDecoder decoder;
aid::audio::AudioData data = decoder.DecodeFromFile("track.mp3");
// data.samples_ — моно float[-1, 1], data.sample_rate_ — например, 44100.
```

```cpp
// Из байтов в памяти (например, из HTTP multipart-запроса):
std::vector<uint8_t> bytes = /* содержимое загруженного файла */;
aid::audio::AudioData data = decoder.DecodeFromBytes(bytes);
```

@section audio_deps Зависимости

- Не зависит от других модулей проекта.
- Внешние библиотеки: `dr_wav`, `minimp3` (обе header-only, подключаются через
  `FetchContent`).
- От `audio` зависит `domain` (`IndexingService` и `MatchingService`
  декодируют входные данные через `AudioDecoder` перед тем, как передать их
  в `core::AudioFingerprintEngine`).
