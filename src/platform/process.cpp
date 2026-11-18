/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "process.h"

#include "logger.h"
#include "lvgl.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <thread>

#if !defined(_WIN32)
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace platform {
namespace {

#if !defined(_WIN32)

void set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool append_available(int fd, std::string& target) {
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            target.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count == 0) {
            return true; // EOF: the child closed its end of the pipe.
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return false;
        }
        return true;
    }
}

#endif

} // namespace

std::string trim_copy(const std::string& value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };
    auto begin = std::find_if(value.begin(), value.end(), is_not_space);
    auto end = std::find_if(value.rbegin(), value.rend(), is_not_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::vector<std::string> split_string(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (const char character : text) {
        if (character == separator) {
            parts.push_back(current);
            current.clear();
        }
        else {
            current.push_back(character);
        }
    }
    parts.push_back(current);
    return parts;
}

std::string format_duration(int seconds, bool pad_minutes) {
    if (seconds <= 0) {
        return "--:--";
    }
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    char buffer[16]{};
    if (hours > 0) {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
    }
    else if (pad_minutes) {
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
    }
    else {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    }
    return buffer;
}

std::string runtime_directory() {
#if defined(_WIN32)
    static const std::string directory = []() {
        std::error_code error;
        auto base = std::filesystem::temp_directory_path(error);
        if (error) {
            base = std::filesystem::path{"."};
        }
        return (base / "cardtube").string();
    }();
#else
    static const std::string directory = "/tmp/cardtube";
#endif
    ensure_directory(directory);
    return directory;
}

void ensure_directory(const std::string& path) {
#if defined(_WIN32)
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
#else
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    ::chmod(path.c_str(), 0700);
#endif
}

bool command_exists(const std::string& name) {
#if defined(_WIN32)
    const auto result = run_command({"where", name}, 4000);
    return result.started && result.exit_code == 0;
#else
    if (name.empty()) {
        return false;
    }
    if (name.find('/') != std::string::npos) {
        return access(name.c_str(), X_OK) == 0;
    }
    const char* path_value = std::getenv("PATH");
    if (!path_value) {
        return false;
    }
    for (const auto& directory : split_string(path_value, ':')) {
        if (directory.empty()) {
            continue;
        }
        const std::string candidate = directory + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return true;
        }
    }
    return false;
#endif
}

CommandResult run_command(const std::vector<std::string>& args, int timeout_ms) {
    CommandResult result;
    if (args.empty()) {
        result.error = "empty command";
        return result;
    }

#if defined(_WIN32)
    (void)timeout_ms;
    result.error = "process launching is not supported on this platform";
    return result;
#else
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.error = std::string("pipe failed: ") + std::strerror(errno);
        if (stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        result.error = std::string("fork failed: ") + std::strerror(errno);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        const int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& argument : args) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    result.started = true;
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    set_nonblocking(stdout_pipe[0]);
    set_nonblocking(stderr_pipe[0]);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 1);
    bool stdout_open = true;
    bool stderr_open = true;

    while (stdout_open || stderr_open) {
        pollfd fds[2]{};
        nfds_t count = 0;
        if (stdout_open) {
            fds[count].fd = stdout_pipe[0];
            fds[count].events = POLLIN;
            ++count;
        }
        if (stderr_open) {
            fds[count].fd = stderr_pipe[0];
            fds[count].events = POLLIN;
            ++count;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            result.timed_out = true;
            break;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        const int poll_timeout = static_cast<int>(std::min<long long>(remaining, 200));
        const int poll_result = poll(fds, count, poll_timeout);
        if (poll_result < 0 && errno != EINTR) {
            break;
        }
        if (poll_result <= 0) {
            continue;
        }

        for (nfds_t index = 0; index < count; ++index) {
            if ((fds[index].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }
            if (fds[index].fd == stdout_pipe[0] && stdout_open) {
                // append_available() returns true on EOF, i.e. the child closed the pipe.
                if (append_available(stdout_pipe[0], result.output)) {
                    stdout_open = false;
                }
            }
            else if (fds[index].fd == stderr_pipe[0] && stderr_open) {
                if (append_available(stderr_pipe[0], result.error)) {
                    stderr_open = false;
                }
            }
        }
    }

    if (result.timed_out) {
        kill(pid, SIGKILL);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    // Extra safety: on timeout the loop bailed out before EOF, so grab whatever is buffered.
    if (result.timed_out) {
        append_available(stdout_pipe[0], result.output);
        append_available(stderr_pipe[0], result.error);
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    return result;
#endif
}

void submit_background(std::function<void()> job) {
    if (!job) {
        return;
    }
    std::thread([job = std::move(job)]() {
        try {
            job();
        }
        catch (const std::exception& exception) {
            LOG_WARN("background task failed: {}", exception.what());
        }
        catch (...) {
            LOG_WARN("background task failed with an unknown exception");
        }
    }).detach();
}

void post_to_ui(std::function<void()> fn) {
    if (!fn) {
        return;
    }
    auto* holder = new std::function<void()>(std::move(fn));
    const auto scheduled = lv_async_call(
        [](void* user_data) {
            auto* callback = static_cast<std::function<void()>*>(user_data);
            if (!callback) {
                return;
            }
            (*callback)();
            delete callback;
        },
        holder);
    if (scheduled != LV_RESULT_OK) {
        delete holder;
    }
}

} // namespace platform
