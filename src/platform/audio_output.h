/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace platform {

struct AudioOutput {
    enum class Kind {
        Auto,
        Sink,
        Bluetooth,
    };

    Kind kind = Kind::Auto;
    std::string sink;         // PulseAudio/PipeWire sink name (Kind::Sink)
    std::string address;      // Bluetooth MAC address (Kind::Bluetooth)
    std::string description;  // Friendly name reported by the system
    bool connected = true;    // Bluetooth: device already routes audio
};

// Queries pactl (PulseAudio/PipeWire) and bluetoothctl. The returned list never
// includes the leading "Auto" entry; callers can add one. Must run off the
// LVGL thread.
std::vector<AudioOutput> list_audio_outputs();

// True when a PulseAudio-compatible server (pactl) is reachable.
bool audio_output_available();

// Name of the current default sink, or empty when unknown.
std::string default_sink_name();

// True when the sink belongs to a Bluetooth device (bluez_output.*).
bool is_bluetooth_sink(const std::string& sink_name);

// Makes sink_name the default sink and moves CardTube's mpv stream to it.
// For a Bluetooth sink the card is switched to its A2DP profile first, so
// `sink_name` can be updated to the recreated PipeWire node.
bool apply_audio_output(std::string& sink_name, std::string& error);

// Moves CardTube's existing streams to sink_name, polling until the stream
// shows up (mpv creates it asynchronously after loadfile). Returns false when
// no stream could be moved within the attempts.
bool route_audio_streams(const std::string& sink_name, int attempts = 20, int delay_ms = 150);

// Connects an already-paired Bluetooth device and waits for its audio sink.
bool connect_bluetooth_output(const std::string& address,
                              std::string& sink_name,
                              std::string& error);

} // namespace platform
