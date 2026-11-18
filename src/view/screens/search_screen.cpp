/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "search_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "screen_util.h"
#include "theme.h"

namespace screen {
namespace {

lv_color_t contrast_text(lv_color_t background) {
    const uint32_t rgb = lv_color_to_u32(background);
    const int luma = static_cast<int>(((rgb >> 16U) & 0xFFU) * 30 +
                                      ((rgb >> 8U) & 0xFFU) * 59 +
                                      (rgb & 0xFFU) * 11) /
                     100;
    return luma > 140 ? lv_color_black() : lv_color_white();
}

} // namespace

void SearchScreen::build_content(lv_obj_t* content) {
    const lv_font_t* caption_font = util::font(assets(), 12);
    const lv_font_t* field_font = util::font(assets(), 14, false);
    const lv_font_t* button_font = util::font(assets(), 12);
    const lv_font_t* hint_font = util::font(assets(), 10, false);

    auto* caption = util::label(content, caption_font, "Search YouTube");
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 10, 4);
    reactive::bind_theme(caption, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    field_ = util::panel(content, 8, 18, 304, 26);
    lv_obj_set_style_radius(field_, 4, 0);
    lv_obj_set_style_border_width(field_, 1, 0);
    lv_obj_set_style_pad_hor(field_, 7, 0);

    query_label_ = util::label(field_, field_font, "");
    lv_label_set_long_mode(query_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(query_label_, 288);
    lv_obj_align(query_label_, LV_ALIGN_LEFT_MID, 0, 0);

    search_button_ = lv_obj_create(content);
    lv_obj_remove_style_all(search_button_);
    lv_obj_set_pos(search_button_, 8, 50);
    lv_obj_set_size(search_button_, 148, 24);
    lv_obj_add_flag(search_button_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(search_button_, 4, 0);
    lv_obj_set_style_bg_opa(search_button_, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(search_button_, search_clicked_cb, LV_EVENT_CLICKED, this);
    auto* search_label = util::label(search_button_, button_font, "Search");
    lv_obj_center(search_label);

    clear_button_ = lv_obj_create(content);
    lv_obj_remove_style_all(clear_button_);
    lv_obj_set_pos(clear_button_, 164, 50);
    lv_obj_set_size(clear_button_, 148, 24);
    lv_obj_add_flag(clear_button_, LV_OBJ_FLAG_CLICKABLE);
    reactive::bind_theme(clear_button_,
                         view_model().dark_mode_subject(),
                         reactive::ThemeRole::Button);
    lv_obj_add_event_cb(clear_button_, clear_clicked_cb, LV_EVENT_CLICKED, this);
    auto* clear_label = util::label(clear_button_, button_font, "Clear");
    lv_obj_center(clear_label);
    reactive::bind_theme(clear_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    hint_ = util::label(content,
                        hint_font,
                        "Type a query, then press ENTER to search");
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(hint_, 300);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_TOP_MID, 0, 78);
    reactive::bind_theme(hint_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    reactive::observe_obj(field_, view_model().query_subject(), query_observer_cb, this);
    reactive::observe_obj(field_,
                          view_model().dark_mode_subject(),
                          theme_observer_cb,
                          this);
    refresh_query();
    apply_theme();
}

void SearchScreen::refresh_query() {
    if (!query_label_) {
        return;
    }

    const std::string query = view_model().model().query();
    std::string text = query.empty() ? "Type to search..." : query;
    if (!query.empty()) {
        text += "|";
    }
    lv_label_set_text(query_label_, text.c_str());
    lv_obj_set_style_text_color(
        query_label_,
        query.empty() ? view::palette(view_model().is_dark_mode()).text_disabled
                      : view::palette(view_model().is_dark_mode()).text,
        0);
}

void SearchScreen::apply_theme() {
    if (!field_ || !search_button_ || !hint_) {
        return;
    }

    const auto colors = view::palette(view_model().is_dark_mode());
    const bool has_query = !view_model().model().query().empty();

    lv_obj_set_style_bg_color(field_, colors.button, 0);
    lv_obj_set_style_bg_opa(field_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(field_, has_query ? colors.primary : colors.border, 0);

    lv_obj_set_style_bg_color(search_button_, colors.primary, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(search_button_, 0),
                                contrast_text(colors.primary),
                                0);
    lv_obj_set_style_text_color(hint_, colors.text_disabled, 0);

    refresh_query();
}

void SearchScreen::query_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<SearchScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh_query();
        screen->apply_theme();
    }
}

void SearchScreen::theme_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<SearchScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->apply_theme();
    }
}

void SearchScreen::search_clicked_cb(lv_event_t* event) {
    auto* screen = static_cast<SearchScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->view_model().start_search();
    }
}

void SearchScreen::clear_clicked_cb(lv_event_t* event) {
    auto* screen = static_cast<SearchScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->view_model().clear_query();
    }
}

} // namespace screen
