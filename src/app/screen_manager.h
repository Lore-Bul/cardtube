/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "base_screen.h"
#include "base_viewmodel.h"
#include "lvgl.h"

#include <array>
#include <memory>

namespace app {

class ScreenManager {
public:
    ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets);
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void start();
    void request_page(model::AppPage page);
    void flush_requested_page();
    // Shows a page immediately, bypassing the async page switch. Used by the
    // demo/screenshot capture path.
    void show_page_now(model::AppPage page);

    lv_obj_t* current_screen() const;
    model::AppPage loaded_page() const;

private:
    screen::BaseScreen* ensure_screen(model::AppPage page);
    std::unique_ptr<screen::BaseScreen> create_screen(model::AppPage page);

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    std::array<std::unique_ptr<screen::BaseScreen>, 5> screens_{};
    screen::BaseScreen* current_screen_{nullptr};
    lv_observer_t* page_observer_ = nullptr;
    model::AppPage requested_page_ = model::AppPage::Search;
    model::AppPage loaded_page_ = model::AppPage::Search;
    bool has_loaded_page_ = false;
    bool page_switch_scheduled_ = false;
};

} // namespace app
