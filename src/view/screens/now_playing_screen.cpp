/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "now_playing_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "screen_util.h"
#include "theme.h"

namespace screen {

void NowPlayingScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    const lv_font_t* title_font = util::font(assets(), 14);
    const lv_font_t* small_font = util::font(assets(), 10, false);
    const lv_font_t* state_font = util::font(assets(), 10);

    thumb_holder_ = util::panel(content, 8, 4, 76, 43);
    lv_obj_set_style_bg_color(thumb_holder_, colors.bar, 0);
    lv_obj_set_style_bg_opa(thumb_holder_, LV_OPA_COVER, 0);

    image_ = lv_image_create(thumb_holder_);
    lv_obj_remove_style_all(image_);
    lv_obj_set_size(image_, 76, 43);
    lv_obj_set_style_radius(image_, 2, 0);
    lv_obj_set_style_clip_corner(image_, true, 0);
    lv_obj_center(image_);

    placeholder_ = util::label(thumb_holder_, util::font(assets(), 12), "no art");
    lv_obj_center(placeholder_);

    title_ = util::label(content, title_font, "");
    lv_label_set_long_mode(title_, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(title_, 92, 3);
    lv_obj_set_size(title_, 220, 30);

    channel_ = util::label(content, small_font, "");
    lv_label_set_long_mode(channel_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(channel_, 92, 33);
    lv_obj_set_size(channel_, 220, 11);

    state_ = util::label(content, state_font, "Stopped");
    lv_obj_set_pos(state_, 92, 44);
    lv_obj_set_size(state_, 220, 11);

    progress_ = lv_bar_create(content);
    lv_obj_remove_style_all(progress_);
    lv_obj_set_pos(progress_, 92, 56);
    lv_obj_set_size(progress_, 220, 5);
    lv_bar_set_range(progress_, 0, 1000);
    lv_bar_set_value(progress_, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(progress_, 2, 0);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(progress_, colors.border, 0);
    lv_obj_set_style_radius(progress_, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_, colors.primary, LV_PART_INDICATOR);

    time_ = util::label(content, small_font, "0:00 / 0:00");
    lv_obj_set_pos(time_, 92, 63);
    lv_obj_set_size(time_, 220, 11);

    hints_ = util::label(content,
                         small_font,
                         "SPACE pause   LEFT/RIGHT seek 10s   V video   N next");
    lv_label_set_long_mode(hints_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(hints_, 8, 78);
    lv_obj_set_size(hints_, 304, 12);
    lv_obj_set_style_text_align(hints_, LV_TEXT_ALIGN_CENTER, 0);

    reactive::observe_obj(content, view_model().now_playing_version_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().position_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().duration_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().progress_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().playing_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().paused_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().thumb_version_subject(), update_cb, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), update_cb, this);
    refresh();
}

void NowPlayingScreen::refresh() {
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto& track = view_model().now_playing();

    lv_obj_set_style_text_color(title_, colors.text, 0);
    lv_obj_set_style_text_color(channel_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(state_, colors.primary, 0);
    lv_obj_set_style_text_color(time_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(hints_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(placeholder_, colors.text_disabled, 0);

    if (!track.valid()) {
        lv_label_set_text(title_, "Nothing is playing yet");
        lv_label_set_text(channel_, "Pick a result or a queued track");
        lv_label_set_text(state_, "Stopped");
        lv_label_set_text(time_, "0:00 / 0:00");
        lv_bar_set_value(progress_, 0, LV_ANIM_OFF);
        lv_image_set_src(image_, nullptr);
        lv_obj_add_flag(image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(placeholder_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(title_, track.title.c_str());
    lv_label_set_text(channel_, track.channel.c_str());

    if (view_model().is_playing()) {
        lv_label_set_text(state_, "Playing");
    }
    else if (view_model().is_paused()) {
        lv_label_set_text(state_, "Paused");
    }
    else {
        lv_label_set_text(state_, "Stopped");
    }

    const int position = lv_subject_get_int(view_model().position_subject());
    const int duration = lv_subject_get_int(view_model().duration_subject());
    const std::string time_text =
        util::clock_text(position) + " / " + util::clock_text(duration);
    lv_label_set_text(time_, time_text.c_str());
    lv_bar_set_value(progress_, lv_subject_get_int(view_model().progress_subject()), LV_ANIM_OFF);

    util::apply_thumbnail(image_, track.id);
    if (util::has_thumbnail(track.id)) {
        lv_obj_remove_flag(image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(placeholder_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(placeholder_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(placeholder_, "no art");
    }
}

void NowPlayingScreen::update_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* screen = static_cast<NowPlayingScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

} // namespace screen
