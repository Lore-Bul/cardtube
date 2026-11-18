/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "queue_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "platform/process.h"
#include "screen_util.h"
#include "theme.h"

#include <algorithm>
#include <cstdint>

namespace screen {

void QueueScreen::build_content(lv_obj_t* content) {
    const lv_font_t* header_font = util::font(assets(), 10);
    const lv_font_t* title_font = util::font(assets(), 12);
    const lv_font_t* small_font = util::font(assets(), 10, false);

    header_ = util::label(content, header_font, "Queue is empty");
    lv_obj_set_pos(header_, 10, 2);
    lv_obj_set_size(header_, 300, 12);

    for (int index = 0; index < kVisibleRows; ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        row.row = util::panel(content, 8, 16 + index * (kRowHeight + 2), 304, kRowHeight);
        lv_obj_set_style_border_width(row.row, 1, 0);
        lv_obj_add_flag(row.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row.row, row_clicked_cb, LV_EVENT_CLICKED, this);

        row.index = util::label(row.row, small_font, "");
        lv_obj_set_pos(row.index, 4, 6);
        lv_obj_set_size(row.index, 20, 11);
        lv_obj_set_style_text_align(row.index, LV_TEXT_ALIGN_RIGHT, 0);

        row.title = util::label(row.row, title_font, "");
        lv_label_set_long_mode(row.title, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(row.title, 28, 4);
        lv_obj_set_size(row.title, 236, 15);

        row.duration = util::label(row.row, small_font, "");
        lv_obj_set_pos(row.duration, 266, 6);
        lv_obj_set_size(row.duration, 32, 11);
        lv_obj_set_style_text_align(row.duration, LV_TEXT_ALIGN_RIGHT, 0);
    }

    empty_label_ = util::label(content, util::font(assets(), 14), "The queue is empty");
    lv_obj_align(empty_label_, LV_ALIGN_CENTER, 0, -6);
    reactive::bind_theme(empty_label_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    empty_hint_ = util::label(content,
                              util::font(assets(), 10, false),
                              "Press Q on a result to queue it");
    lv_obj_align(empty_hint_, LV_ALIGN_CENTER, 0, 14);
    reactive::bind_theme(empty_hint_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    reactive::observe_obj(content, view_model().queue_version_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().queue_selection_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().queue_index_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), update_cb, this);
    refresh();
}

void QueueScreen::refresh() {
    const auto& queue = view_model().queue();
    const auto colors = view::palette(view_model().is_dark_mode());
    const int total = static_cast<int>(queue.size());
    const int selected = view_model().queue_selection();
    const int playing = view_model().queue_index();

    lv_obj_set_style_text_color(header_, colors.text_disabled, 0);

    if (total == 0) {
        lv_label_set_text(header_, "Queue is empty");
    }
    else {
        std::string header = std::to_string(total) + (total == 1 ? " track" : " tracks");
        if (playing >= 0 && playing < total) {
            header += "  -  playing " + std::to_string(playing + 1);
        }
        lv_label_set_text(header_, header.c_str());
    }

    const bool empty = total == 0;
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

    const int top = std::clamp(selected - 1, 0, std::max(0, total - kVisibleRows));

    for (int slot = 0; slot < kVisibleRows; ++slot) {
        auto& row = rows_[static_cast<std::size_t>(slot)];
        const int item = top + slot;
        if (item >= total) {
            lv_obj_add_flag(row.row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_remove_flag(row.row, LV_OBJ_FLAG_HIDDEN);
        const auto& track = queue[static_cast<std::size_t>(item)];
        const bool is_selected = item == selected;
        const bool is_playing = item == playing;

        lv_obj_set_style_bg_color(row.row, is_selected ? colors.button : colors.surface, 0);
        lv_obj_set_style_bg_opa(row.row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row.row, is_selected ? colors.primary : colors.border, 0);

        lv_label_set_text_fmt(row.index, "%d.", item + 1);
        lv_obj_set_style_text_color(row.index, is_playing ? colors.primary : colors.text_disabled, 0);

        lv_label_set_text(row.title, track.title.c_str());
        lv_obj_set_style_text_color(row.title, is_playing ? colors.primary : colors.text, 0);

        lv_label_set_text(row.duration, platform::format_duration(track.duration_s).c_str());
        lv_obj_set_style_text_color(row.duration, colors.text_disabled, 0);

        lv_obj_set_user_data(row.row,
                             reinterpret_cast<void*>(static_cast<uintptr_t>(item)));
    }
}

void QueueScreen::update_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<QueueScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

void QueueScreen::row_clicked_cb(lv_event_t* event) {
    auto* screen = static_cast<QueueScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (!screen || !row) {
        return;
    }

    const int index = static_cast<int>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row)));
    screen->view_model().play_queue_index(index);
}

} // namespace screen
