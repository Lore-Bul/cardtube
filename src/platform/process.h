/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace platform {

struct CommandResult {
    bool started = false;
    bool timed_out = false;
    int exit_code = -1;
    std::string output;
    std::string error;
};

// Runs a program without a shell and captures stdout/stderr separately.
CommandResult run_command(const std::vector<std::string>& args, int timeout_ms = 20000);

bool command_exists(const std::string& name);

// Detached worker thread; exceptions are swallowed. Jobs must not touch LVGL.
void submit_background(std::function<void()> job);

// Runs fn on the LVGL thread during a later lv_timer_handler() pass.
void post_to_ui(std::function<void()> fn);

std::string trim_copy(const std::string& value);
std::string lowercase_copy(std::string value);
std::vector<std::string> split_string(const std::string& text, char separator);
std::string format_duration(int seconds, bool pad_minutes = false);

// /tmp/cardtube on device builds, created on demand.
std::string runtime_directory();
void ensure_directory(const std::string& path);

} // namespace platform
