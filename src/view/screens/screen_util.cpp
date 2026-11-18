/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_util.h"

#include "asset_manager.h"
#include "ytdlp.h"

#include "src/misc/cache/instance/lv_image_cache.h"

#include <cstdio>

namespace screen::util {
namespace {

const lv_font_t* montserrat_for(int size) {
    if (size <= 10) {
        return &lv_font_montserrat_10;
    }
    if (size <= 12) {
        return &lv_font_montserrat_12;
    }
    if (size <= 14) {
        return &lv_font_montserrat_14;
    }
    if (size <= 16) {
        return &lv_font_montserrat_16;
    }
    if (size <= 18) {
        return &lv_font_montserrat_18;
    }
    if (size <= 20) {
        return &lv_font_montserrat_20;
    }
    return &lv_font_montserrat_24;
}

} // namespace

const lv_font_t* font(app::AssetManager& assets, int size, bool bold) {
    auto* loaded = assets.load_standard_font(
        static_cast<uint32_t>(size),
        bold ? app::StandardFontWeight::Bold : app::StandardFontWeight::Regular);
    return loaded ? loaded : montserrat_for(size);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, const char* text) {
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    return label;
}

lv_obj_t* panel(lv_obj_t* parent, int x, int y, int width, int height) {
    auto* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 3, 0);
    return obj;
}

std::string clock_text(int seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    char buffer[16]{};
    if (hours > 0) {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
    }
    else {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    }
    return buffer;
}

bool has_thumbnail(const std::string& video_id) {
    return !video_id.empty() && platform::ytdlp::thumbnail_cached(video_id);
}

void apply_thumbnail(lv_obj_t* image, const std::string& video_id) {
    if (!image) {
        return;
    }

    if (!has_thumbnail(video_id)) {
        lv_image_set_src(image, nullptr);
        return;
    }

    const std::string source = platform::ytdlp::thumbnail_source(video_id);
    if (source.empty()) {
        lv_image_set_src(image, nullptr);
        return;
    }

    // Drop the decoded cache entry: the file may have been downloaded after the
    // previous refresh, and setting the same path would keep the old bitmap.
    lv_image_cache_drop(source.c_str());
    lv_image_set_src(image, source.c_str());
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_COVER);
}

} // namespace screen::util
