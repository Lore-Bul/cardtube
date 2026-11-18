/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace config {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1" || normalized == "on") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0" || normalized == "off") {
        result = false;
        return true;
    }
    return false;
}

bool parse_int(const std::string& value, int& result) {
    const auto normalized = trim(value);
    if (normalized.empty()) {
        return false;
    }
    int parsed = 0;
    const auto end = normalized.data() + normalized.size();
    const auto parsed_result = std::from_chars(normalized.data(), end, parsed);
    if (parsed_result.ec != std::errc{} || parsed_result.ptr != end) {
        return false;
    }
    result = parsed;
    return true;
}

} // namespace

bool is_valid_quality(const std::string& quality) {
    return quality == "best" || quality == "480p" || quality == "360p";
}

std::string normalize_quality(const std::string& quality) {
    const auto normalized = lowercase(trim(quality));
    if (normalized == "480p" || normalized == "480") {
        return "480p";
    }
    if (normalized == "360p" || normalized == "360") {
        return "360p";
    }
    return "best";
}

std::string normalize_language(const std::string& language) {
    const auto normalized = lowercase(trim(language));
    if (normalized == "it" || normalized == "ita" || normalized == "italian" ||
        normalized == "italiano") {
        return "it";
    }
    return "en";
}

std::string normalize_keyboard_layout(const std::string& layout) {
    const auto normalized = lowercase(trim(layout));
    if (normalized == "it" || normalized == "ita" || normalized == "italian" ||
        normalized == "italiano") {
        return "it";
    }
    return "us";
}

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        const auto value = line.substr(separator + 1);

        if (section != "application" && section != "playback") {
            continue;
        }

        if (key == "dark_mode" || key == "theme") {
            if (!parse_bool(value, config.dark_mode)) {
                error = "invalid dark_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (key == "video_mode") {
            if (!parse_bool(value, config.video_mode)) {
                error = "invalid video_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (key == "autoplay_next") {
            if (!parse_bool(value, config.autoplay_next)) {
                error = "invalid autoplay_next value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (key == "volume") {
            int parsed = 0;
            if (!parse_int(value, parsed)) {
                error = "invalid volume value at line " + std::to_string(line_number);
                return false;
            }
            config.volume = std::clamp(parsed, 0, 100);
        }
        else if (key == "quality") {
            config.quality = normalize_quality(value);
        }
        else if (key == "language") {
            config.language = normalize_language(value);
        }
        else if (key == "keyboard" || key == "keyboard_layout") {
            config.keyboard_layout = normalize_keyboard_layout(value);
        }
        else if (key == "audio_output" || key == "output") {
            config.audio_output = trim(value);
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open file for writing";
        return false;
    }

    output << "# CardTube configuration\n"
           << "[application]\n"
           << "dark_mode=" << (config.dark_mode ? "yes" : "no") << "\n"
           << "video_mode=" << (config.video_mode ? "yes" : "no") << "\n"
           << "autoplay_next=" << (config.autoplay_next ? "yes" : "no") << "\n"
           << "volume=" << std::clamp(config.volume, 0, 100) << "\n"
           << "quality=" << normalize_quality(config.quality) << "\n"
           << "language=" << normalize_language(config.language) << "\n"
           << "keyboard=" << normalize_keyboard_layout(config.keyboard_layout) << "\n"
           << "audio_output=" << trim(config.audio_output) << "\n";

    if (!output) {
        error = "failed while writing file";
        return false;
    }

    return true;
}

} // namespace config
