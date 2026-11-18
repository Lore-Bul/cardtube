/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::mpv_ipc {

// Minimal blocking client for mpv's JSON IPC socket. All calls are expected to
// be serialized by the caller (mpv_player owns a single worker).
class Client {
public:
    Client() = default;
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect_to(const std::string& socket_path, int timeout_ms = 3000);
    void close();
    bool connected() const;

    // Returns the raw JSON payload of a successful get_property reply.
    bool get_property(const std::string& name, std::string& value, int timeout_ms = 600);

    bool set_property(const std::string& name, const std::string& json_value, int timeout_ms = 600);

    // `command` is the JSON array, e.g. ["loadfile","url","replace"].
    bool command(const std::string& command, int timeout_ms = 1200);

    // Fire and forget, used for shutdown.
    void send_command_no_reply(const std::string& command);

private:
    bool wait_readable(int timeout_ms) const;
    bool read_line(std::string& line, int timeout_ms);
    bool send_all(const std::string& data);
    bool wait_for_reply(int request_id, std::string& payload, int timeout_ms);

    int fd_ = -1;
    int request_id_ = 0;
    std::string buffer_;
};

bool parse_number(const std::string& raw, double& value);
bool parse_bool(const std::string& raw, bool& value);

} // namespace platform::mpv_ipc
