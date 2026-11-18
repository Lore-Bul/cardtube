/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "ytdlp.h"

#include "logger.h"
#include "process.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace platform::ytdlp {
namespace {

constexpr const char* kSearchSeparator = "\t";

std::string sanitize(std::string value) {
    value.erase(std::remove_if(value.begin(),
                               value.end(),
                               [](unsigned char character) {
                                   return character < 0x20 || character == 0x7f;
                               }),
                value.end());
    return platform::trim_copy(value);
}

int parse_duration(const std::string& raw) {
    if (raw.empty() || raw == "NA" || raw == "None" || raw == "null") {
        return 0;
    }
    try {
        const double seconds = std::stod(raw);
        if (seconds <= 0.0 || seconds > 24.0 * 3600.0) {
            return 0;
        }
        return static_cast<int>(seconds + 0.5);
    }
    catch (...) {
        return 0;
    }
}

bool usable_id(const std::string& id) {
    if (id.size() < 6 || id.size() > 32) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
    });
}

} // namespace

std::string DependencyStatus::summary() const {
    std::string missing;
    const auto append = [&missing](const char* name) {
        if (!missing.empty()) {
            missing += ", ";
        }
        missing += name;
    };
    if (!ytdlp) {
        append("yt-dlp");
    }
    if (!mpv) {
        append("mpv");
    }
    if (!curl) {
        append("curl");
    }
    return missing.empty() ? "yt-dlp, mpv and curl are available" : "Missing: " + missing;
}

SearchResult search(const std::string& query, int limit, int timeout_ms) {
    SearchResult result;
    const std::string trimmed = platform::trim_copy(query);
    if (trimmed.empty()) {
        result.error = "the search box is empty";
        return result;
    }
    if (!platform::command_exists("yt-dlp")) {
        result.error = "yt-dlp is not installed";
        return result;
    }

    if (limit <= 0) {
        limit = 12;
    }

    const std::string print_template =
        "%(id)s" + std::string(kSearchSeparator) + "%(title)s" + kSearchSeparator +
        "%(channel,uploader,uploader_id)s" + kSearchSeparator + "%(duration)s";

    const auto command = platform::run_command({"yt-dlp",
                                               "--flat-playlist",
                                               "--no-warnings",
                                               "--ignore-errors",
                                               "--no-progress",
                                               "--skip-download",
                                               "--print",
                                               print_template,
                                               "ytsearch" + std::to_string(limit) + ":" + trimmed},
                                              timeout_ms);

    if (command.timed_out) {
        result.error = "yt-dlp timed out";
        return result;
    }
    if (!command.started) {
        result.error = command.error.empty() ? "unable to start yt-dlp" : command.error;
        return result;
    }

    std::size_t line_start = 0;
    const std::string& output = command.output;
    while (line_start < output.size()) {
        auto line_end = output.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = output.size();
        }
        std::string line = output.substr(line_start, line_end - line_start);
        line_start = line_end + 1;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const auto fields = platform::split_string(line, kSearchSeparator[0]);
        if (fields.size() < 4) {
            continue;
        }

        model::TrackInfo track;
        track.id = sanitize(fields[0]);
        if (!usable_id(track.id)) {
            continue;
        }
        track.title = sanitize(fields[1]);
        if (track.title.empty() || track.title == "NA") {
            track.title = "Untitled video";
        }
        track.channel = sanitize(fields[2]);
        if (track.channel.empty() || track.channel == "NA") {
            track.channel = "YouTube";
        }
        track.duration_s = parse_duration(fields[3]);

        const auto duplicate = std::find_if(result.tracks.begin(),
                                            result.tracks.end(),
                                            [&track](const model::TrackInfo& existing) {
                                                return existing.id == track.id;
                                            });
        if (duplicate == result.tracks.end()) {
            result.tracks.push_back(std::move(track));
        }
    }

    if (result.tracks.empty()) {
        const std::string details = platform::trim_copy(command.error);
        result.error = details.empty() ? "no results found" : details;
        return result;
    }

    result.ok = true;
    return result;
}

bool fetch_thumbnail(const std::string& video_id, const std::string& output_path) {
    if (video_id.empty() || output_path.empty() || !platform::command_exists("curl")) {
        return false;
    }

    const std::filesystem::path path(output_path);
    std::error_code filesystem_error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), filesystem_error);
    }
    std::filesystem::remove(path, filesystem_error);

    const std::string base = "https://i.ytimg.com/vi/" + video_id + "/";
    for (const char* name : {"mqdefault.jpg", "default.jpg"}) {
        const auto command = platform::run_command(
            {"curl", "-fsSL", "--max-time", "12", "-o", output_path, base + name}, 15000);
        if (!command.started || command.exit_code != 0) {
            continue;
        }

        std::error_code size_error;
        const auto size = std::filesystem::file_size(path, size_error);
        if (!size_error && size > 512) {
            return true;
        }
    }

    std::filesystem::remove(path, filesystem_error);
    return false;
}

std::string thumbnail_path(const std::string& video_id) {
    if (video_id.empty()) {
        return {};
    }
    return platform::runtime_directory() + "/thumbs/" + video_id + ".jpg";
}

std::string thumbnail_source(const std::string& video_id) {
    const std::string path = thumbnail_path(video_id);
    if (path.empty()) {
        return {};
    }

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return "A:" + normalized;
}

bool thumbnail_cached(const std::string& video_id) {
    const std::string path = thumbnail_path(video_id);
    if (path.empty()) {
        return false;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    return !error && size > 512;
}

std::string version() {
    const auto command = platform::run_command({"yt-dlp", "--version"}, 6000);
    if (!command.started || command.exit_code != 0) {
        return {};
    }
    return platform::trim_copy(command.output);
}

UpdateResult update(int timeout_ms) {
    UpdateResult result;
    if (!platform::command_exists("yt-dlp")) {
        result.message = "yt-dlp is not installed";
        return result;
    }

    const auto command = platform::run_command({"yt-dlp", "-U"}, timeout_ms);
    const std::string output = platform::trim_copy(command.output + "\n" + command.error);
    result.ok = command.started && !command.timed_out && command.exit_code == 0;
    if (command.timed_out) {
        result.message = "yt-dlp update timed out";
    }
    else if (result.ok) {
        result.message = output.empty() ? "yt-dlp is already up to date" : output;
    }
    else if (!output.empty()) {
        result.message = output;
    }
    else {
        result.message = "yt-dlp could not update itself (try `sudo pip install -U yt-dlp`)";
    }

    // Keep messages compact: the toast only has a couple of lines.
    constexpr std::size_t kMaxMessage = 220;
    if (result.message.size() > kMaxMessage) {
        result.message.resize(kMaxMessage);
        result.message += "...";
    }
    return result;
}

DependencyStatus dependencies() {
    DependencyStatus status;
    status.ytdlp = platform::command_exists("yt-dlp");
    status.mpv = platform::command_exists("mpv");
    status.curl = platform::command_exists("curl");
    return status;
}

std::string format_selector(bool video, const std::string& quality) {
    if (!video) {
        return "bestaudio/best";
    }

    int height = 480;
    if (quality == "360p") {
        height = 360;
    }
    else if (quality == "480p") {
        height = 480;
    }

    const std::string limit = std::to_string(height);
    return "bestvideo[height<=" + limit + "]+bestaudio/best[height<=" + limit + "]/best";
}

} // namespace platform::ytdlp
