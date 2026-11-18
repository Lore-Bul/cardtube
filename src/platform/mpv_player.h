/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::mpv {

struct Snapshot {
    bool daemon_running = false;
    bool media_loaded = false;
    bool playing = false;
    bool paused = false;
    bool ended = false;
    double position_s = 0.0;
    double duration_s = 0.0;
};

// Starts the persistent, audio-only mpv daemon. Returns false when mpv is
// missing or the IPC socket never becomes available.
bool start(int volume_percent);
void stop();
bool running();
bool available();

// Loads a YouTube (or any yt-dlp supported) URL into the daemon.
bool load(const std::string& url, const std::string& quality);
void stop_playback();

void toggle_pause();
void set_pause(bool paused);
void seek_relative(double seconds);
void set_volume(int volume_percent);

// Routes the audio output to a PulseAudio/PipeWire sink ("pipewire/<sink>",
// falling back to "pulse/<sink>"). An empty name restores automatic selection.
void set_audio_device(const std::string& sink);

Snapshot snapshot();
void clear_ended();

} // namespace platform::mpv
