/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_output.h"

#include "process.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <sstream>
#include <thread>

namespace platform {
namespace {

bool starts_with(const std::string& text, const char* prefix) {
    const auto length = std::strlen(prefix);
    return text.size() >= length && text.compare(0, length, prefix) == 0;
}

std::string value_after_colon(const std::string& line) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return {};
    }
    return trim_copy(line.substr(colon + 1));
}

std::string combined_output(const CommandResult& result) {
    std::string text = trim_copy(result.output);
    const auto error = trim_copy(result.error);
    if (!error.empty()) {
        if (!text.empty()) {
            text += " ";
        }
        text += error;
    }
    return text;
}

std::string mac_token(const std::string& address) {
    std::string token = lowercase_copy(address);
    std::replace(token.begin(), token.end(), ':', '_');
    return token;
}

std::string query_sinks_text() {
    const auto result = run_command({"pactl", "list", "sinks"}, 6000);
    if (!result.started || result.exit_code != 0) {
        return {};
    }
    return result.output;
}

std::vector<AudioOutput> parse_sinks(const std::string& text) {
    std::vector<AudioOutput> sinks;
    AudioOutput current;
    bool active = false;

    const auto flush = [&]() {
        if (active && !current.sink.empty()) {
            if (current.description.empty()) {
                current.description = current.sink;
            }
            current.kind = AudioOutput::Kind::Sink;
            current.connected = true;
            sinks.push_back(current);
        }
        current = AudioOutput{};
        active = false;
    };

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        const auto trimmed = trim_copy(line);
        if (starts_with(trimmed, "Sink #")) {
            flush();
            active = true;
            continue;
        }
        if (!active) {
            continue;
        }
        if (current.sink.empty() && starts_with(trimmed, "Name:")) {
            current.sink = value_after_colon(trimmed);
        }
        else if (current.description.empty() && starts_with(trimmed, "Description:")) {
            current.description = value_after_colon(trimmed);
        }
    }
    flush();
    return sinks;
}

std::vector<AudioOutput> query_connected_sinks() {
    return parse_sinks(query_sinks_text());
}

std::vector<AudioOutput> query_paired_bluetooth(const std::vector<AudioOutput>& sinks) {
    std::vector<AudioOutput> devices;

    const auto result = run_command({"bluetoothctl", "devices", "Paired"}, 6000);
    if (!result.started || result.exit_code != 0) {
        return devices;
    }

    std::istringstream stream(result.output);
    std::string line;
    while (std::getline(stream, line)) {
        const auto trimmed = trim_copy(line);
        if (!starts_with(trimmed, "Device ")) {
            continue;
        }
        const auto rest = trimmed.substr(std::strlen("Device "));
        const auto space = rest.find(' ');
        if (space == std::string::npos) {
            continue;
        }
        const auto address = rest.substr(0, space);
        auto name = trim_copy(rest.substr(space + 1));
        if (address.find(':') == std::string::npos) {
            continue;
        }
        if (name.empty()) {
            name = address;
        }

        const auto token = mac_token(address);
        const bool connected = std::any_of(
            sinks.begin(), sinks.end(), [&token](const AudioOutput& sink) {
                return lowercase_copy(sink.sink).find(token) != std::string::npos;
            });
        if (connected) {
            continue; // Already offered through its PipeWire sink.
        }

        AudioOutput device;
        device.kind = AudioOutput::Kind::Bluetooth;
        device.address = address;
        device.description = name;
        device.connected = false;
        devices.push_back(std::move(device));
    }
    return devices;
}

bool run_pactl(const std::vector<std::string>& args, int timeout_ms, std::string& error) {
    std::vector<std::string> command{"pactl"};
    command.insert(command.end(), args.begin(), args.end());
    const auto result = run_command(command, timeout_ms);
    if (!result.started) {
        error = result.error.empty() ? "pactl could not be started" : result.error;
        return false;
    }
    if (result.exit_code != 0) {
        error = combined_output(result);
        if (error.empty()) {
            error = "pactl exited with code " + std::to_string(result.exit_code);
        }
        return false;
    }
    return true;
}

std::vector<int> cardtube_streams(const std::string& text) {
    std::vector<int> ids;
    int current = -1;
    bool ours = false;

    const auto flush = [&]() {
        if (current >= 0 && ours) {
            ids.push_back(current);
        }
        current = -1;
        ours = false;
    };

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        const auto trimmed = trim_copy(line);
        if (starts_with(trimmed, "Sink Input #")) {
            flush();
            current = std::atoi(trimmed.c_str() + std::strlen("Sink Input #"));
            continue;
        }
        if (current < 0) {
            continue;
        }
        if (trimmed.find("application.name = \"CardTube\"") != std::string::npos ||
            trimmed.find("application.name = \"mpv\"") != std::string::npos ||
            trimmed.find("application.process.binary = \"mpv\"") != std::string::npos) {
            ours = true;
        }
    }
    flush();
    return ids;
}

void move_cardtube_streams(const std::string& sink_name) {
    const auto result = run_command({"pactl", "list", "sink-inputs"}, 6000);
    if (!result.started || result.exit_code != 0) {
        return;
    }

    std::string ignored;
    for (const int id : cardtube_streams(result.output)) {
        run_pactl({"move-sink-input", std::to_string(id), sink_name}, 5000, ignored);
    }
}

} // namespace

bool audio_output_available() {
    return command_exists("pactl");
}

bool is_bluetooth_sink(const std::string& sink_name) {
    const auto lower = lowercase_copy(sink_name);
    return lower.find("bluez") != std::string::npos;
}

std::string default_sink_name() {
    const auto result = run_command({"pactl", "get-default-sink"}, 5000);
    if (!result.started || result.exit_code != 0) {
        return {};
    }
    return trim_copy(result.output);
}

std::vector<AudioOutput> list_audio_outputs() {
    if (!audio_output_available()) {
        return {};
    }

    auto sinks = query_connected_sinks();
    auto bluetooth = query_paired_bluetooth(sinks);
    sinks.insert(sinks.end(),
                 std::make_move_iterator(bluetooth.begin()),
                 std::make_move_iterator(bluetooth.end()));
    return sinks;
}

bool apply_audio_output(const std::string& sink_name, std::string& error) {
    error.clear();
    if (sink_name.empty()) {
        error = "empty sink name";
        return false;
    }
    if (!run_pactl({"set-default-sink", sink_name}, 6000, error)) {
        return false;
    }

    std::string ignored;
    run_pactl({"set-sink-mute", sink_name, "0"}, 5000, ignored);
    move_cardtube_streams(sink_name);
    return true;
}

bool connect_bluetooth_output(const std::string& address,
                              std::string& sink_name,
                              std::string& error) {
    error.clear();
    sink_name.clear();
    if (address.empty()) {
        error = "empty bluetooth address";
        return false;
    }

    const auto result = run_command({"bluetoothctl", "connect", address}, 20000);
    const auto output = combined_output(result);
    const auto lower = lowercase_copy(output);
    const bool connected = result.started &&
                           (result.exit_code == 0 ||
                            lower.find("successful") != std::string::npos ||
                            lower.find("already connected") != std::string::npos);
    if (!connected) {
        error = output.empty() ? "bluetoothctl connect failed" : output;
        return false;
    }

    const auto token = mac_token(address);
    for (int attempt = 0; attempt < 10; ++attempt) {
        for (const auto& sink : query_connected_sinks()) {
            if (lowercase_copy(sink.sink).find(token) != std::string::npos) {
                sink_name = sink.sink;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    error = "the device connected but no audio sink appeared";
    return false;
}

} // namespace platform
