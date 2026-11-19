/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform::video {

// Starts a one-shot mpv process that draws directly to the connected DRM/KMS
// connector (the 320x170 SPI panel on the Cardputer Zero). The caller must
// pause LVGL rendering while active().
bool play(const std::string& url,
          const std::string& quality,
          int volume_percent,
          std::string& error);

bool active();
int exit_code();
void stop();

} // namespace platform::video
