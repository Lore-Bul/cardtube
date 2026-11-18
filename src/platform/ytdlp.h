/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"

#include <string>
#include <vector>

namespace platform::ytdlp {

struct SearchResult {
    bool ok = false;
    bool cancelled = false;
    std::vector<model::TrackInfo> tracks;
    std::string error;
};

struct UpdateResult {
    bool ok = false;
    std::string message;
};

struct DependencyStatus {
    bool ytdlp = false;
    bool mpv = false;
    bool curl = false;

    bool ready() const {
        return ytdlp && mpv && curl;
    }

    std::string summary() const;
};

// Runs `yt-dlp ytsearch<limit>:<query>` and parses the flat playlist output.
SearchResult search(const std::string& query, int limit = 12, int timeout_ms = 30000);

// Downloads the medium quality thumbnail for a video id. Returns true when the
// file exists and looks like an image.
bool fetch_thumbnail(const std::string& video_id, const std::string& output_path);

// Cache location for a video thumbnail, for instance
// /tmp/cardtube/thumbs/dQw4w9WgXcQ.jpg
std::string thumbnail_path(const std::string& video_id);

// Same path, formatted for LVGL's stdio filesystem driver ("A:/tmp/...").
std::string thumbnail_source(const std::string& video_id);

bool thumbnail_cached(const std::string& video_id);

// `yt-dlp --version`, empty when yt-dlp is missing or fails.
std::string version();

// Runs `yt-dlp -U`. Some installations are managed by the package manager and
// refuse self updates; the message explains what happened.
UpdateResult update(int timeout_ms = 180000);

DependencyStatus dependencies();

// mpv --ytdl-format value for audio or video playback.
std::string format_selector(bool video, const std::string& quality);

} // namespace platform::ytdlp
