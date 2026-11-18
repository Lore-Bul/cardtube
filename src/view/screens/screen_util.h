/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl.h"

#include <string>

namespace app {
class AssetManager;
}

namespace screen::util {

// Standard UI font with a Montserrat fallback (desktop returns nullptr).
const lv_font_t* font(app::AssetManager& assets, int size, bool bold = true);

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, const char* text);

// Flat, borderless container used for cards and rows.
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int width, int height);

// "0:07", "3:45", "1:02:09".
std::string clock_text(int seconds);

// True when the thumbnail for this video id is already in the cache.
bool has_thumbnail(const std::string& video_id);

// Assigns the cached JPEG (or clears the image when it is missing/corrupt).
void apply_thumbnail(lv_obj_t* image, const std::string& video_id);

} // namespace screen::util
