/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpv_ipc.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace platform::mpv_ipc {
namespace {

std::string make_request(const std::string& command, int request_id) {
    return "{\"command\":" + command + ",\"request_id\":" + std::to_string(request_id) + "}\n";
}

std::string extract_payload(const std::string& line) {
    const auto data_key = line.find("\"data\":");
    if (data_key == std::string::npos) {
        return {};
    }

    const auto start = data_key + 7;
    if (start >= line.size()) {
        return {};
    }

    const char first = line[start];
    if (first == '{' || first == '[') {
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (std::size_t index = start; index < line.size(); ++index) {
            const char character = line[index];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                }
                else if (character == '\\') {
                    escaped = true;
                }
                else if (character == '"') {
                    in_string = false;
                }
                continue;
            }
            if (character == '"') {
                in_string = true;
            }
            else if (character == '{' || character == '[') {
                ++depth;
            }
            else if (character == '}' || character == ']') {
                --depth;
                if (depth == 0) {
                    return line.substr(start, index - start + 1);
                }
            }
        }
        return line.substr(start);
    }

    auto end = line.find_first_of(",}", start);
    if (end == std::string::npos) {
        end = line.size();
    }
    return line.substr(start, end - start);
}

} // namespace

Client::~Client() {
    close();
}

bool Client::connect_to(const std::string& socket_path, int timeout_ms) {
    close();
#if defined(_WIN32)
    (void)socket_path;
    (void)timeout_ms;
    return false;
#else
    sockaddr_un address{};
    if (socket_path.empty() || socket_path.size() >= sizeof(address.sun_path)) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return false;
        }

        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            fd_ = fd;
            return true;
        }

        ::close(fd);
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        ::usleep(100 * 1000);
    }
#endif
}

void Client::close() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
    buffer_.clear();
}

bool Client::connected() const {
    return fd_ >= 0;
}

bool Client::wait_readable(int timeout_ms) const {
#if defined(_WIN32)
    (void)timeout_ms;
    return false;
#else
    if (fd_ < 0) {
        return false;
    }
    pollfd descriptor{};
    descriptor.fd = fd_;
    descriptor.events = POLLIN;
    for (;;) {
        const int result = poll(&descriptor, 1, timeout_ms);
        if (result > 0) {
            return (descriptor.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
        }
        if (result == 0) {
            return false;
        }
        if (errno != EINTR) {
            return false;
        }
    }
#endif
}

bool Client::read_line(std::string& line, int timeout_ms) {
#if defined(_WIN32)
    (void)line;
    (void)timeout_ms;
    return false;
#else
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const auto newline = buffer_.find('\n');
        if (newline != std::string::npos) {
            line = buffer_.substr(0, newline);
            buffer_.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return false;
        }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (!wait_readable(static_cast<int>(std::max<long long>(1, remaining)))) {
            return false;
        }

        char chunk[2048];
        const ssize_t count = ::recv(fd_, chunk, sizeof(chunk), 0);
        if (count > 0) {
            buffer_.append(chunk, static_cast<std::size_t>(count));
            continue;
        }
        if (count == 0) {
            return false;
        }
        if (errno != EINTR && errno != EAGAIN) {
            return false;
        }
    }
#endif
}

bool Client::send_all(const std::string& data) {
#if defined(_WIN32)
    (void)data;
    return false;
#else
    if (fd_ < 0) {
        return false;
    }
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t count =
            ::send(fd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (count > 0) {
            sent += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        return false;
    }
    return true;
#endif
}

bool Client::wait_for_reply(int request_id, std::string& payload, int timeout_ms) {
    const std::string marker = "\"request_id\":" + std::to_string(request_id);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::string line;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return false;
        }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (!read_line(line, static_cast<int>(std::max<long long>(1, remaining)))) {
            return false;
        }
        if (line.find(marker) == std::string::npos) {
            continue;
        }
        if (line.find("\"error\":\"success\"") == std::string::npos) {
            return false;
        }
        payload = extract_payload(line);
        return true;
    }
}

bool Client::get_property(const std::string& name, std::string& value, int timeout_ms) {
    const int id = ++request_id_;
    const std::string command = "[\"get_property\",\"" + name + "\"]";
    if (!send_all(make_request(command, id))) {
        return false;
    }
    std::string payload;
    if (!wait_for_reply(id, payload, timeout_ms)) {
        return false;
    }
    value = std::move(payload);
    return true;
}

bool Client::set_property(const std::string& name, const std::string& json_value, int timeout_ms) {
    const int id = ++request_id_;
    const std::string command =
        "[\"set_property\",\"" + name + "\"," + json_value + "]";
    if (!send_all(make_request(command, id))) {
        return false;
    }
    std::string payload;
    return wait_for_reply(id, payload, timeout_ms);
}

bool Client::command(const std::string& command, int timeout_ms) {
    const int id = ++request_id_;
    if (!send_all(make_request(command, id))) {
        return false;
    }
    std::string payload;
    return wait_for_reply(id, payload, timeout_ms);
}

void Client::send_command_no_reply(const std::string& command) {
    const int id = ++request_id_;
    send_all(make_request(command, id));
}

bool parse_number(const std::string& raw, double& value) {
    const std::string text = raw;
    if (text.empty() || text == "null" || text == "false") {
        return false;
    }
    try {
        std::size_t used = 0;
        const double parsed = std::stod(text, &used);
        if (used == 0) {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parse_bool(const std::string& raw, bool& value) {
    if (raw == "true") {
        value = true;
        return true;
    }
    if (raw == "false") {
        value = false;
        return true;
    }
    return false;
}

} // namespace platform::mpv_ipc
