/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::video {

// Starts a one-shot mpv process that draws directly to the framebuffer. The
// caller must pause LVGL rendering while active(). Falls back to the DRM/GPU
// output when fbdev is not usable.
bool play(const std::string& url,
          const std::string& quality,
          int volume_percent,
          std::string& error);

bool active();
int exit_code();
void stop();

} // namespace platform::video
