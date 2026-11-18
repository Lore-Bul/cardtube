/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "results_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "platform/process.h"
#include "screen_util.h"
#include "theme.h"

#include <algorithm>
#include <cstdint>

namespace screen {

void ResultsScreen::build_content(lv_obj_t* content) {
    const lv_font_t* title_font = util::font(assets(), 12);
    const lv_font_t* meta_font = util::font(assets(), 10, false);
    const lv_font_t* thumb_font = util::font(assets(), 10);

    for (int index = 0; index < kVisibleRows; ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        row.row = util::panel(content, 8, 4 + index * kRowStep, 304, kRowHeight);
        lv_obj_set_style_border_width(row.row, 1, 0);
        lv_obj_add_flag(row.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row.row, row_clicked_cb, LV_EVENT_CLICKED, this);

        row.thumb_holder = util::panel(row.row, 3, 2, 38, 23);
        row.image = lv_image_create(row.thumb_holder);
        lv_obj_remove_style_all(row.image);
        lv_obj_set_size(row.image, 38, 23);
        lv_obj_set_style_radius(row.image, 2, 0);
        lv_obj_set_style_clip_corner(row.image, true, 0);
        lv_obj_center(row.image);

        row.placeholder = util::label(row.thumb_holder, thumb_font, "");
        lv_obj_center(row.placeholder);

        row.title = util::label(row.row, title_font, "");
        lv_label_set_long_mode(row.title, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(row.title, 46, 2);
        lv_obj_set_size(row.title, 250, 14);

        row.meta = util::label(row.row, meta_font, "");
        lv_label_set_long_mode(row.meta, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(row.meta, 46, 15);
        lv_obj_set_size(row.meta, 250, 11);
    }

    empty_label_ = util::label(content, util::font(assets(), 14), "No results yet");
    lv_obj_align(empty_label_, LV_ALIGN_CENTER, 0, -8);
    reactive::bind_theme(empty_label_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    empty_hint_ = util::label(content,
                              util::font(assets(), 10, false),
                              "Type a query on the Search page");
    lv_obj_align(empty_hint_, LV_ALIGN_CENTER, 0, 12);
    reactive::bind_theme(empty_hint_,
                         view_model().dark_mode_subject(),
                         reactive::ThemeRole::Text);

    reactive::observe_obj(content,
                          view_model().results_version_subject(),
                          update_cb,
                          this);
    reactive::observe_obj(content,
                          view_model().selected_result_subject(),
                          update_cb,
                          this);
    reactive::observe_obj(content, view_model().thumb_version_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), update_cb, this);
    refresh();
}

void ResultsScreen::refresh() {
    const auto& tracks = view_model().results();
    const auto colors = view::palette(view_model().is_dark_mode());
    const int total = static_cast<int>(tracks.size());
    const int selected = view_model().selected_result();

    const bool empty = tracks.empty();
    if (empty_label_) {
        if (empty) {
            lv_obj_remove_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(empty_label_, colors.text, 0);
            lv_obj_set_style_text_color(empty_hint_, colors.text_disabled, 0);
        }
        else {
            lv_obj_add_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const int top = std::clamp(selected - 1, 0, std::max(0, total - kVisibleRows));

    for (int slot = 0; slot < kVisibleRows; ++slot) {
        auto& row = rows_[static_cast<std::size_t>(slot)];
        const int item = top + slot;
        if (item >= total) {
            lv_obj_add_flag(row.row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_remove_flag(row.row, LV_OBJ_FLAG_HIDDEN);
        const auto& track = tracks[static_cast<std::size_t>(item)];
        const bool is_selected = item == selected;

        lv_obj_set_style_bg_color(row.row, is_selected ? colors.button : colors.surface, 0);
        lv_obj_set_style_bg_opa(row.row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row.row, is_selected ? colors.primary : colors.border, 0);

        lv_label_set_text(row.title, track.title.c_str());
        lv_obj_set_style_text_color(row.title, colors.text, 0);

        const std::string meta = track.channel + "  -  " +
                                 platform::format_duration(track.duration_s);
        lv_label_set_text(row.meta, meta.c_str());
        lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);

        lv_obj_set_style_bg_color(row.thumb_holder, colors.bar, 0);
        lv_obj_set_style_bg_opa(row.thumb_holder, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(row.placeholder, colors.text_disabled, 0);

        util::apply_thumbnail(row.image, track.id);
        const bool has_image = util::has_thumbnail(track.id);
        if (has_image) {
            lv_obj_remove_flag(row.image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(row.placeholder, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(row.image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(row.placeholder, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(row.placeholder, "%d", item + 1);
        }

        lv_obj_set_user_data(row.row,
                             reinterpret_cast<void*>(static_cast<uintptr_t>(item)));
    }
}

void ResultsScreen::update_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<ResultsScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

void ResultsScreen::row_clicked_cb(lv_event_t* event) {
    auto* screen = static_cast<ResultsScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (!screen || !row) {
        return;
    }

    const int index = static_cast<int>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row)));
    screen->view_model().play_result(index);
}

} // namespace screen
