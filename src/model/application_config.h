/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace config {

struct ApplicationConfig {
    bool dark_mode = true;
    bool video_mode = false;
    bool autoplay_next = true;
    int volume = 80;
    std::string quality = "best";
    std::string language = "en";
    std::string audio_output;
};

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error);

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error);

// Returns true when the quality token is one of the supported values.
bool is_valid_quality(const std::string& quality);

// Best -> Best, 480p -> 480p, 360p -> 360p.
std::string normalize_quality(const std::string& quality);

// Language token: "it"/"italian" -> it, anything else -> en.
std::string normalize_language(const std::string& language);

} // namespace config
