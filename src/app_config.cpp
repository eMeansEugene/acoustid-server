//
// Created by evgen on 20.07.2026.
//

#include "app_config.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace aid {

using json = nlohmann::json;

AppConfig AppConfig::Defaults() {
    AppConfig config;
    config.peak.zone_frames_ = 43;
    return config;
}

AppConfig AppConfig::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Invalid JSON in config file: " + std::string(e.what()));
    }

    AppConfig config = Defaults();

    // --- database ---
    if (j.contains("database")) {
        const auto& db = j["database"];
        if (db.contains("path")) config.db_path = db["path"].get<std::string>();
    }

    // --- fft ---
    if (j.contains("fft")) {
        const auto& fft = j["fft"];
        if (fft.contains("frame_size")) config.fft.frame_size_ = fft["frame_size"].get<std::size_t>();
        if (fft.contains("hop_size")) config.fft.hop_size_ = fft["hop_size"].get<std::size_t>();
    }

    // --- peak_extractor ---
    if (j.contains("peak_extractor")) {
        const auto& pe = j["peak_extractor"];
        if (pe.contains("frame_radius")) config.peak.frame_radius_ = pe["frame_radius"].get<std::size_t>();
        if (pe.contains("bin_radius")) config.peak.bin_radius_ = pe["bin_radius"].get<std::size_t>();
        if (pe.contains("offset_db")) config.peak.offset_db_ = pe["offset_db"].get<float>();
        if (pe.contains("zone_frames")) config.peak.zone_frames_ = pe["zone_frames"].get<std::size_t>();
        if (pe.contains("peak_limit")) config.peak.peak_limit_ = pe["peak_limit"].get<std::size_t>();
    }

    // --- hash_generator ---
    if (j.contains("hash_generator")) {
        const auto& hg = j["hash_generator"];
        if (hg.contains("max_target_offset")) config.hash.max_target_offset_ = hg["max_target_offset"].get<std::size_t>();
        if (hg.contains("max_targets_per_anchor")) config.hash.max_targets_per_anchor_ = hg["max_targets_per_anchor"].get<std::size_t>();
        if (hg.contains("freq_bin_limit")) config.hash.freq_bin_limit_ = hg["freq_bin_limit"].get<std::size_t>();
    }

    // --- voting ---
    if (j.contains("voting")) {
        const auto& v = j["voting"];
        if (v.contains("min_confidence")) config.voting.min_confidence_ = v["min_confidence"].get<double>();
    }

    // --- server ---
    if (j.contains("server")) {
        const auto& s = j["server"];
        if (s.contains("port")) config.server.port = s["port"].get<uint16_t>();
        if (s.contains("api_key")) config.server.admin_api_key = s["api_key"].get<std::string>();
        if (s.contains("workers")) {
            // workers хранится отдельно, не в HttpServerConfig — передаём через отдельное поле ниже
        }
        if (s.contains("max_upload_bytes")) config.server.max_upload_bytes = s["max_upload_bytes"].get<std::size_t>();
    }

    return config;
}

}  // namespace aid
