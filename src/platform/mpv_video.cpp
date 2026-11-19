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
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
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

std::string sysfs_value(const std::string& path) {
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

// Returns the DRM card that drives a connected connector. On the Cardputer
// Zero the 320x170 SPI panel is card1 while card0 is an unused HDMI output,
// so the card is discovered at runtime instead of being hard-coded. Connectors
// whose name contains SPI are preferred when more than one is connected.
std::string detect_drm_device() {
    const std::string root = "/sys/class/drm/";
    DIR* directory = ::opendir(root.c_str());
    if (directory == nullptr) {
        return {};
    }

    std::string detected;
    bool detected_spi = false;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(directory)) != nullptr) {
        const std::string name = entry->d_name;
        // Connectors look like "card1-SPI-1" or "card0-HDMI-A-1".
        const std::size_t dash = name.find('-');
        if (name.rfind("card", 0) != 0 || dash == std::string::npos) {
            continue;
        }
        if (sysfs_value(root + name + "/status") != "connected") {
            continue;
        }

        const std::string device = "/dev/dri/" + name.substr(0, dash);
        if (::access(device.c_str(), F_OK) != 0) {
            continue;
        }

        const bool spi = name.find("SPI") != std::string::npos ||
                         name.find("spi") != std::string::npos;
        if (detected.empty() || (spi && !detected_spi)) {
            detected = device;
            detected_spi = spi;
        }
    }
    ::closedir(directory);
    return detected;
}
#endif

#if defined(_WIN32)
std::string detect_drm_device() {
    return {};
}
#endif

#if defined(_WIN32)
bool spawn(const std::string& url,
           const std::string& quality,
           int volume_percent,
           const std::vector<std::string>& video_args,
           const std::string& log_name,
           std::string& error) {
    (void)url;
    (void)quality;
    (void)volume_percent;
    (void)video_args;
    (void)log_name;
    error = "video playback is only available on the device";
    return false;
}
#else
bool spawn(const std::string& url,
           const std::string& quality,
           int volume_percent,
           const std::vector<std::string>& video_args,
           const std::string& log_name,
           std::string& error) {
    const std::string directory = platform::runtime_directory();
    const std::string log_path = directory + "/" + log_name;

    std::vector<std::string> args = {
        "mpv",
        // --no-terminal / --really-quiet silence stdout and stderr as well,
        // which would leave the log file empty when mpv refuses a VO.
        "--log-file=" + log_path,
        "--msg-level=all=info",
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

    // mpv 0.40 dropped the fbdev VO. The Cardputer Zero panel is driven by
    // KMS/DRM, so pass the connected card directly to mpv's software DRM VO.
    // Fall back to card1 when sysfs is unavailable (known device layout).
    std::string drm_device = detect_drm_device();
    if (drm_device.empty()) {
        drm_device = "/dev/dri/card1";
    }
    LOG_INFO("video output: DRM device {}", drm_device);

    std::string drm_error;
    if (spawn(url, quality, volume_percent,
              {"--vo=drm", "--drm-device=" + drm_device},
              "mpv-video-drm.log", drm_error)) {
        error.clear();
        return true;
    }

    LOG_WARN("DRM video output failed, trying gpu/drm: {}", drm_error);
    std::string gpu_error;
    if (spawn(url, quality, volume_percent,
              {"--vo=gpu", "--gpu-context=drm", "--drm-device=" + drm_device},
              "mpv-video-gpu.log", gpu_error)) {
        error.clear();
        return true;
    }

    error = "DRM: " + drm_error + " / gpu: " + gpu_error;
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
