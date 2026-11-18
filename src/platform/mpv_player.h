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

// Routes the audio output to a PulseAudio/PipeWire sink. Tries "pulse/<sink>"
// first (always present when pactl works) and then "pipewire/<sink>". An empty
// name restores automatic selection. Returns true when mpv accepted a device.
bool set_audio_device(const std::string& sink);

// Logs the audio properties mpv is really using (audio-device, current-ao,
// audio-params). Call after a stream started to diagnose silent playback.
void log_audio_status();

Snapshot snapshot();
void clear_ended();

} // namespace platform::mpv
