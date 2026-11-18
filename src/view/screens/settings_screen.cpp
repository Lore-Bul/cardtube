/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "screen_util.h"
#include "theme.h"

#include <cstdint>

namespace screen {

void SettingsScreen::build_content(lv_obj_t* content) {
    const lv_font_t* name_font = util::font(assets(), 12);
    const lv_font_t* value_font = util::font(assets(), 12);

    list_ = lv_obj_create(content);
    lv_obj_remove_style_all(list_);
    lv_obj_set_pos(list_, 0, 0);
    lv_obj_set_size(list_, 320, 94);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(list_, 0, 0);

    for (int index = 0; index < kRowCount; ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        row.row = lv_obj_create(list_);
        lv_obj_remove_style_all(row.row);
        lv_obj_set_pos(row.row, 0, index * kRowHeight);
        lv_obj_set_size(row.row, 320, kRowHeight);
        lv_obj_clear_flag(row.row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row.row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_width(row.row, 2, 0);
        lv_obj_set_style_border_side(row.row, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_add_event_cb(row.row, row_clicked_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(row.row,
                             reinterpret_cast<void*>(static_cast<uintptr_t>(index)));

        row.name = util::label(row.row, name_font, "");
        lv_obj_set_pos(row.name, 10, 0);
        lv_obj_set_size(row.name, 150, kRowHeight);

        row.value = util::label(row.row, value_font, "");
        lv_label_set_long_mode(row.value, LV_LABEL_LONG_DOT);
        lv_obj_set_pos(row.value, 158, 0);
        lv_obj_set_size(row.value, 152, kRowHeight);
        lv_obj_set_style_text_align(row.value, LV_TEXT_ALIGN_RIGHT, 0);
    }

    reactive::observe_obj(content, view_model().settings_version_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().settings_selection_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), update_cb, this);
    refresh();
}

void SettingsScreen::refresh() {
    const auto colors = view::palette(view_model().is_dark_mode());
    const int selected = view_model().settings_selection();

    for (int index = 0; index < kRowCount; ++index) {
        auto& row = rows_[static_cast<std::size_t>(index)];
        const bool is_selected = index == selected;

        lv_obj_set_style_bg_color(row.row, is_selected ? colors.button : colors.surface, 0);
        lv_obj_set_style_bg_opa(row.row, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(row.row, colors.primary, 0);
        lv_obj_set_style_border_opa(row.row, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

        lv_label_set_text(row.name, view_model().setting_label(index).c_str());
        lv_obj_set_style_text_color(row.name, is_selected ? colors.text : colors.text_disabled, 0);

        lv_label_set_text(row.value, view_model().setting_value(index).c_str());
        lv_obj_set_style_text_color(row.value,
                                    is_selected ? colors.primary : colors.text_disabled,
                                    0);
    }

    if (selected >= 0 && selected < kRowCount && rows_[static_cast<std::size_t>(selected)].row) {
        lv_obj_scroll_to_view(rows_[static_cast<std::size_t>(selected)].row, LV_ANIM_OFF);
    }
}

void SettingsScreen::update_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<SettingsScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

void SettingsScreen::row_clicked_cb(lv_event_t* event) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (!screen || !row) {
        return;
    }

    const int index = static_cast<int>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row)));
    screen->view_model().select_settings(index);
    screen->view_model().activate_setting();
}

} // namespace screen
