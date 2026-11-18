/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpv_video.h"

#include "logger.h"
#include "process.h"
#include "ytdlp.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform::video {
namespace {

#if !defined(_WIN32)
pid_t g_pid = -1;
int g_exit_code = 0;
#endif

#if defined(_WIN32)
bool spawn(const std::string& url,
           const std::string& quality,
           int volume_percent,
           const std::vector<std::string>& video_args,
           std::string& error) {
    (void)url;
    (void)quality;
    (void)volume_percent;
    (void)video_args;
    error = "video playback is only available on the device";
    return false;
}
#else
bool spawn(const std::string& url,
           const std::string& quality,
           int volume_percent,
           const std::vector<std::string>& video_args,
           std::string& error) {
    const std::string directory = platform::runtime_directory();
    const std::string log_path = directory + "/mpv-video.log";

    std::vector<std::string> args = {
        "mpv",
        "--no-terminal",
        "--really-quiet",
        "--no-config",
        "--no-input-terminal",
        "--input-default-bindings=no",
        "--audio-display=no",
        "--cache=yes",
        "--demuxer-max-bytes=32MiB",
        "--demuxer-max-back-bytes=16MiB",
        "--hwdec=auto-safe",
        "--volume=" + std::to_string(std::max(0, std::min(100, volume_percent))),
        "--ytdl-format=" + platform::ytdlp::format_selector(true, quality),
    };
    args.insert(args.end(), video_args.begin(), video_args.end());
    args.push_back(url);

    const pid_t pid = ::fork();
    if (pid < 0) {
        error = "unable to start mpv";
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
    g_exit_code = 0;

    // Give mpv time to open the output device. When it exits immediately the
    // requested video backend is not usable on this system.
    for (int tick = 0; tick < 30; ++tick) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int status = 0;
        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result == pid || result < 0) {
            if (WIFEXITED(status)) {
                g_exit_code = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status)) {
                g_exit_code = 128 + WTERMSIG(status);
            }
            else {
                g_exit_code = -1;
            }
            g_pid = -1;
            error = "mpv exited early (code " + std::to_string(g_exit_code) +
                    "), see " + log_path;
            return false;
        }
    }

    LOG_INFO("video mpv started (pid {})", static_cast<long long>(pid));
    return true;
}
#endif

} // namespace

bool play(const std::string& url,
          const std::string& quality,
          int volume_percent,
          std::string& error) {
    error.clear();
    stop();
    if (url.empty()) {
        error = "nothing to play";
        return false;
    }
    if (!platform::command_exists("mpv")) {
        error = "mpv is not installed";
        return false;
    }

    // The framebuffer output shares /dev/fb0 with LVGL and is the most reliable
    // option on the Cardputer Zero. KMS/DRM is the fallback when fbdev is busy.
    if (spawn(url, quality, volume_percent, {"--vo=fbdev"}, error)) {
        return true;
    }

    LOG_WARN("fbdev video output failed, trying DRM: {}", error);
    std::string drm_error;
    if (spawn(url, quality, volume_percent, {"--vo=gpu", "--gpu-context=drm"}, drm_error)) {
        error.clear();
        return true;
    }

    error += " / DRM: " + drm_error;
    return false;
}

bool active() {
#if defined(_WIN32)
    return false;
#else
    if (g_pid <= 0) {
        return false;
    }

    int status = 0;
    const pid_t result = ::waitpid(g_pid, &status, WNOHANG);
    if (result == g_pid || result < 0) {
        if (result == g_pid) {
            if (WIFEXITED(status)) {
                g_exit_code = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status)) {
                g_exit_code = 128 + WTERMSIG(status);
            }
        }
        g_pid = -1;
        return false;
    }
    return true;
#endif
}

int exit_code() {
#if defined(_WIN32)
    return 0;
#else
    return g_exit_code;
#endif
}

void stop() {
#if !defined(_WIN32)
    if (g_pid <= 0) {
        return;
    }

    ::kill(g_pid, SIGTERM);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
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
#endif
}

} // namespace platform::video
