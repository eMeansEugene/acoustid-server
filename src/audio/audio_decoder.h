//
// Created by evgen on 13.07.2026.
//

#ifndef ACOUSTID_SERVER_AUDIO_DECODER_H
#define ACOUSTID_SERVER_AUDIO_DECODER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aid::audio {
    /// Результат декодирования аудиофайла.
    struct AudioData {
        std::vector<float> samples_;  ///< Моно-сэмплы (первый канал, если стерео).
        std::size_t sample_rate_;     ///< Частота дискретизации (например, 44100).
        float duration_sec_;          ///< Длительность в секундах.
    };

    /// Декодирует MP3 и WAV в моно float-сэмплы.
    ///
    /// Формат определяется по магическим байтам содержимого, не по расширению.
    /// Если файл стерео — берётся только первый канал.
    class AudioDecoder {
    public:
        /// Прочитать файл с диска и декодировать.
        /// @throws std::runtime_error при ошибке чтения или неизвестном формате.
        AudioData DecodeFromFile(const std::string& path) const;

        /// Декодировать из байтов в памяти.
        /// @throws std::runtime_error при ошибке декодирования или неизвестном формате.
        AudioData DecodeFromBytes(const std::vector<uint8_t>& bytes) const;

    private:
        /// Определяет формат по первым байтам и вызывает нужный декодер.
        enum class Format { kWav, kMp3, kUnknown };
        static Format DetectFormat(const std::vector<uint8_t>& bytes);

        static AudioData DecodeWav(const std::vector<uint8_t>& bytes);
        static AudioData DecodeMp3(const std::vector<uint8_t>& bytes);

        /// Из интерливленных многоканальных сэмплов извлекает первый канал.
        static std::vector<float> ExtractFirstChannel(const float* interleaved,
                                                       std::size_t total_samples,
                                                       std::size_t num_channels);
    };
}

#endif // ACOUSTID_SERVER_AUDIO_DECODER_H
