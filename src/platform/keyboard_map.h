/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace platform {

// Translates a Linux evdev key code into the LVGL key/character expected by the
// views. Returns 0 when the key produces no text and has no binding.
//
// `cardputer_keyboard` selects the built-in tca8418c profile: its kernel driver
// already resolves the Sym layer and reports vendor key codes that overlap with
// ordinary evdev codes (26 is "!" there but "[" on a USB keyboard).
uint32_t map_evdev_key(uint16_t code, bool shift, bool cardputer_keyboard);

// Font-independent regression check of the key tables above. Returns false and
// fills `error` on the first mismatch.
bool keyboard_map_self_test(std::string& error);

} // namespace platform
