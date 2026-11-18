/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpv_player.h"

#include "logger.h"
#include "mpv_ipc.h"
#include "process.h"
#include "ytdlp.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform::mpv {
namespace {

std::mutex g_client_mutex;
std::mutex g_snapshot_mutex;
mpv_ipc::Client g_client;
Snapshot g_snapshot;
std::atomic<bool> g_worker_running{false};
std::thread g_worker;
std::string g_socket_path;
std::atomic<bool> g_has_media{false};
int g_volume = 80;

#if !defined(_WIN32)
pid_t g_pid = -1;
#endif

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(character);
                break;
        }
    }
    return escaped;
}

void publish_snapshot(const Snapshot& snapshot) {
    std::lock_guard<std::mutex> lock(g_snapshot_mutex);
    g_snapshot = snapshot;
}

void update_from_ipc() {
    Snapshot snapshot;
    snapshot.daemon_running = true;
    snapshot.media_loaded = g_has_media.load();

    double number = 0.0;
    bool flag = false;
    std::string raw;

    {
        std::lock_guard<std::mutex> lock(g_client_mutex);
        if (!g_client.connected()) {
            return;
        }

        if (g_client.get_property("time-pos", raw) && mpv_ipc::parse_number(raw, number)) {
            snapshot.position_s = std::max(0.0, number);
        }
        if (g_client.get_property("duration", raw) && mpv_ipc::parse_number(raw, number)) {
            snapshot.duration_s = std::max(0.0, number);
        }
        if (g_client.get_property("pause", raw) && mpv_ipc::parse_bool(raw, flag)) {
            snapshot.paused = flag;
        }
    }

    bool idle = false;
    {
        std::lock_guard<std::mutex> lock(g_client_mutex);
        if (g_client.get_property("idle-active", raw) && mpv_ipc::parse_bool(raw, flag)) {
            idle = flag;
        }
    }

    if (g_has_media.load() && idle) {
        snapshot.ended = true;
    }
    snapshot.playing = g_has_media.load() && !idle && !snapshot.paused && !snapshot.ended;
    if (snapshot.ended) {
        snapshot.paused = false;
    }

    publish_snapshot(snapshot);
}

void worker_loop() {
    while (g_worker_running.load()) {
        for (int tick = 0; tick < 8 && g_worker_running.load(); ++tick) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!g_worker_running.load()) {
            break;
        }
        update_from_ipc();
    }
}

#if defined(_WIN32)
bool spawn_daemon(int volume_percent) {
    (void)volume_percent;
    return false;
}
#else
bool spawn_daemon(int volume_percent) {
    const std::string directory = platform::runtime_directory();
    g_socket_path = directory + "/mpv-" + std::to_string(static_cast<long long>(::getpid())) + ".sock";
    ::unlink(g_socket_path.c_str());

    const std::string log_path = directory + "/mpv.log";
    const std::string volume = std::to_string(std::max(0, std::min(100, volume_percent)));

    std::vector<std::string> args = {
        "mpv",
        "--idle=yes",
        "--no-video",
        "--no-terminal",
        "--input-terminal=no",
        "--input-default-bindings=no",
        "--really-quiet",
        "--no-config",
        "--audio-display=no",
"--audio-client-name=CardTube",
        "--gapless-audio=yes",
        "--cache=yes",
        "--demuxer-max-bytes=24MiB",
        "--demuxer-max-back-bytes=8MiB",
        "--volume=" + volume,
        "--ytdl-format=" + ytdlp::format_selector(false, "best"),
        "--input-ipc-server=" + g_socket_path,
    };

    const pid_t pid = ::fork();
    if (pid < 0) {
        LOG_WARN("unable to fork mpv: {}", std::strerror(errno));
        return false;
    }

    if (pid == 0) {
        const int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::close(devnull);
        }
        const int log_fd =
            ::open(log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0600);
        if (log_fd >= 0) {
            ::dup2(log_fd, STDOUT_FILENO);
            ::dup2(log_fd, STDERR_FILENO);
            ::close(log_fd);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& argument : args) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    g_pid = pid;
    if (!g_client.connect_to(g_socket_path, 5000)) {
        LOG_WARN("mpv IPC socket did not appear at {}", g_socket_path);
        ::kill(pid, SIGTERM);
        int status = 0;
        ::waitpid(pid, &status, 0);
        g_pid = -1;
        return false;
    }

    LOG_INFO("mpv daemon started (pid {})", static_cast<long long>(pid));
    return true;
}
#endif

void wait_for_child(int timeout_ms) {
#if !defined(_WIN32)
    if (g_pid <= 0) {
        return;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int status = 0;
    for (;;) {
        const pid_t result = ::waitpid(g_pid, &status, WNOHANG);
        if (result == g_pid || result < 0) {
            g_pid = -1;
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            ::kill(g_pid, SIGKILL);
            ::waitpid(g_pid, &status, 0);
            g_pid = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
#else
    (void)timeout_ms;
#endif
}

} // namespace

bool available() {
    return platform::command_exists("mpv");
}

bool running() {
#if defined(_WIN32)
    return false;
#else
    if (g_pid <= 0) {
        return false;
    }
    int status = 0;
    const pid_t result = ::waitpid(g_pid, &status, WNOHANG);
    if (result == g_pid || result < 0) {
        g_pid = -1;
        return false;
    }
    return true;
#endif
}

bool start(int volume_percent) {
    if (running()) {
        set_volume(volume_percent);
        return true;
    }
    if (!available()) {
        return false;
    }

    g_volume = std::max(0, std::min(100, volume_percent));
    g_has_media.store(false);
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshot = Snapshot{};
        g_snapshot.daemon_running = true;
    }

    if (!spawn_daemon(g_volume)) {
        return false;
    }

    g_worker_running.store(true);
    g_worker = std::thread(worker_loop);
    set_volume(g_volume);
    return true;
}

void stop() {
    g_worker_running.store(false);
    if (g_worker.joinable()) {
        g_worker.join();
    }

#if !defined(_WIN32)
    {
        std::lock_guard<std::mutex> lock(g_client_mutex);
        if (g_client.connected()) {
            g_client.send_command_no_reply("[\"quit\"]");
            g_client.close();
        }
    }
    wait_for_child(1200);
    if (!g_socket_path.empty()) {
        ::unlink(g_socket_path.c_str());
        g_socket_path.clear();
    }
#endif

    g_has_media.store(false);
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshot = Snapshot{};
    }
}

bool load(const std::string& url, const std::string& quality) {
    if (url.empty() || !running()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_client_mutex);
        g_client.set_property("ytdl-format",
                              "\"" + json_escape(ytdlp::format_selector(false, quality)) + "\"",
                              800);
        const std::string command = "[\"loadfile\",\"" + json_escape(url) + "\",\"replace\"]";
        if (!g_client.command(command, 1500)) {
            return false;
        }
        g_client.set_property("pause", "false", 500);
    }

    g_has_media.store(true);
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshot = Snapshot{};
        g_snapshot.daemon_running = true;
        g_snapshot.media_loaded = true;
        g_snapshot.playing = true;
    }
    return true;
}

void stop_playback() {
    g_has_media.store(false);
    {
        std::lock_guard<std::mutex> lock(g_client_mutex);
        if (g_client.connected()) {
            g_client.command("[\"stop\"]", 800);
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshot = Snapshot{};
        g_snapshot.daemon_running = running();
    }
}

void toggle_pause() {
    std::lock_guard<std::mutex> lock(g_client_mutex);
    if (g_client.connected()) {
        g_client.command("[\"cycle\",\"pause\"]", 600);
    }
}

void set_pause(bool paused) {
    std::lock_guard<std::mutex> lock(g_client_mutex);
    if (g_client.connected()) {
        g_client.set_property("pause", paused ? "true" : "false", 600);
    }
}

void seek_relative(double seconds) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.3f", seconds);
    std::lock_guard<std::mutex> lock(g_client_mutex);
    if (g_client.connected()) {
        g_client.command(std::string("[\"seek\",") + buffer + ",\"relative\"]", 600);
    }
}

void set_volume(int volume_percent) {
    g_volume = std::max(0, std::min(100, volume_percent));
    std::lock_guard<std::mutex> lock(g_client_mutex);
    if (g_client.connected()) {
        g_client.set_property("volume", std::to_string(g_volume), 600);
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_snapshot_mutex);
    return g_snapshot;
}

void clear_ended() {
    std::lock_guard<std::mutex> lock(g_snapshot_mutex);
    g_snapshot.ended = false;
    g_snapshot.playing = false;
}

} // namespace platform::mpv
