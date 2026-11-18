/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"

#include <cstdint>

namespace view::widgets {
namespace {

struct TabSpec {
    const char* label;
    model::AppPage page;
};

constexpr std::array<TabSpec, 5> kTabs = {{
    {"Search", model::AppPage::Search},
    {"Results", model::AppPage::Results},
    {"Player", model::AppPage::NowPlaying},
    {"Queue", model::AppPage::Queue},
    {"Settings", model::AppPage::Settings},
}};

std::size_t tab_index(model::AppPage page) {
    for (std::size_t index = 0; index < kTabs.size(); ++index) {
        if (kTabs[index].page == page) {
            return index;
        }
    }
    return 0;
}

} // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() {
    deactivate();

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
    if (theme_observer_) {
        lv_observer_remove(theme_observer_);
        theme_observer_ = nullptr;
    }
}

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(core_obj_, 2, 0);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    const lv_font_t* font = assets_.load_standard_font(12);
    if (!font) {
        font = &lv_font_montserrat_12;
    }

    for (std::size_t index = 0; index < kTabs.size(); ++index) {
        auto* tab = lv_obj_create(core_obj_);
        lv_obj_remove_style_all(tab);
        lv_obj_set_size(tab, 62, 26);
        lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(tab, 4, 0);
        lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tab, 2, 0);
        lv_obj_set_style_border_side(tab, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_opa(tab, LV_OPA_TRANSP, 0);
        lv_obj_set_user_data(tab,
                             reinterpret_cast<void*>(static_cast<uintptr_t>(kTabs[index].page)));
        lv_obj_add_event_cb(tab, tab_clicked_cb, LV_EVENT_CLICKED, this);
        tabs_[index] = tab;

        auto* label = lv_label_create(tab);
        lv_label_set_text(label, kTabs[index].label);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_center(label);
        labels_[index] = label;
    }

    page_observer_ = reactive::observe_obj(core_obj_,
                                          view_model_.current_page_subject(),
                                          update_cb,
                                          this);
    theme_observer_ = reactive::observe_obj(core_obj_,
                                           view_model_.dark_mode_subject(),
                                           update_cb,
                                           this);
    apply_styles();
}

void NavBar::activate() {
    active_ = true;
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        platform::register_nav_button(index, tabs_[index]);
    }
    platform::set_nav_shortcut_mode(view_model_.nav_shortcut_allowed());
}

void NavBar::deactivate() {
    active_ = false;
    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        platform::unregister_nav_button(index, tabs_[index]);
    }
}

void NavBar::apply_styles() {
    const auto colors = view::palette(view_model_.is_dark_mode());
    const auto selected = tab_index(view_model_.current_page());

    for (std::size_t index = 0; index < tabs_.size(); ++index) {
        const bool is_selected = index == selected;
        lv_obj_t* tab = tabs_[index];
        lv_obj_t* label = labels_[index];
        if (!tab || !label) {
            continue;
        }

        lv_obj_set_style_bg_opa(tab, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(tab, colors.button, 0);
        lv_obj_set_style_border_opa(tab, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(tab, colors.primary, 0);
        lv_obj_set_style_text_color(label,
                                    is_selected ? colors.primary : colors.text_disabled,
                                    0);
    }
}

void NavBar::tab_clicked_cb(lv_event_t* event) {
    auto* nav_bar = static_cast<NavBar*>(lv_event_get_user_data(event));
    auto* tab = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (!nav_bar || !tab) {
        return;
    }

    const auto page = static_cast<model::AppPage>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(tab)));
    nav_bar->view_model_.set_page(page);
}

void NavBar::update_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);

    auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (!nav_bar) {
        return;
    }

    nav_bar->apply_styles();
    if (nav_bar->active_) {
        platform::set_nav_shortcut_mode(nav_bar->view_model_.nav_shortcut_allowed());
    }
}

} // namespace view::widgets
