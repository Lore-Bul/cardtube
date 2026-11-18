/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_manager.h"

#include "now_playing_screen.h"
#include "queue_screen.h"
#include "results_screen.h"
#include "search_screen.h"
#include "settings_screen.h"

#include <cstddef>

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
    auto* manager = static_cast<ScreenManager*>(user_data);
    if (manager) {
        manager->flush_requested_page();
    }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (!manager) {
        return;
    }

    manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

} // namespace

ScreenManager::ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

ScreenManager::~ScreenManager() {
    lv_async_call_cancel(flush_requested_page_async, this);

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }

    if (current_screen_) {
        current_screen_->deactivate();
        current_screen_ = nullptr;
    }

    for (auto& screen : screens_) {
        screen.reset();
    }
}

void ScreenManager::start() {
    if (page_observer_) {
        return;
    }

    page_observer_ = lv_subject_add_observer_with_target(view_model_.current_page_subject(),
                                                         current_page_observer,
                                                         this,
                                                         nullptr);

    requested_page_ = view_model_.model().current_page();
    flush_requested_page();
}

std::unique_ptr<screen::BaseScreen> ScreenManager::create_screen(model::AppPage page) {
    switch (page) {
        case model::AppPage::Search:
            return std::make_unique<screen::SearchScreen>(view_model_, assets_);
        case model::AppPage::Results:
            return std::make_unique<screen::ResultsScreen>(view_model_, assets_);
        case model::AppPage::NowPlaying:
            return std::make_unique<screen::NowPlayingScreen>(view_model_, assets_);
        case model::AppPage::Queue:
            return std::make_unique<screen::QueueScreen>(view_model_, assets_);
        case model::AppPage::Settings:
            return std::make_unique<screen::SettingsScreen>(view_model_, assets_);
    }
    return nullptr;
}

screen::BaseScreen* ScreenManager::ensure_screen(model::AppPage page) {
    const auto index = static_cast<std::size_t>(page);
    if (index >= screens_.size()) {
        return nullptr;
    }

    auto& slot = screens_[index];
    if (!slot) {
        slot = create_screen(page);
        if (slot) {
            slot->init();
        }
    }
    return slot.get();
}

void ScreenManager::request_page(model::AppPage page) {
    requested_page_ = page;

    if (page_switch_scheduled_) {
        return;
    }

    page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
    page_switch_scheduled_ = false;

    if (has_loaded_page_ && requested_page_ == loaded_page_) {
        return;
    }

    auto* next = ensure_screen(requested_page_);
    if (!next || !next->root()) {
        return;
    }

    if (current_screen_) {
        current_screen_->deactivate();
    }

    lv_screen_load(next->root());
    current_screen_ = next;
    loaded_page_ = requested_page_;
    has_loaded_page_ = true;
    next->activate();
}

void ScreenManager::show_page_now(model::AppPage page) {
    requested_page_ = page;
    flush_requested_page();
}

void ScreenManager::rebuild() {
    if (!page_observer_) {
        return;
    }

    // Load a throwaway screen first: LVGL must never reference a screen that is
    // about to be deleted.
    lv_obj_t* placeholder = lv_obj_create(nullptr);
    if (placeholder) {
        lv_obj_remove_style_all(placeholder);
        lv_screen_load(placeholder);
    }

    if (current_screen_) {
        current_screen_->deactivate();
        current_screen_ = nullptr;
    }
    for (auto& screen : screens_) {
        screen.reset();
    }

    const auto page = view_model_.model().current_page();
    requested_page_ = page;
    loaded_page_ = page;
    has_loaded_page_ = false;
    flush_requested_page();

    if (placeholder && lv_obj_is_valid(placeholder)) {
        lv_obj_delete(placeholder);
    }
}

lv_obj_t* ScreenManager::current_screen() const {
    return current_screen_ ? current_screen_->root() : nullptr;
}

model::AppPage ScreenManager::loaded_page() const {
    return loaded_page_;
}

} // namespace app
