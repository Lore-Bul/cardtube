/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"

#include <array>

namespace app {
class AssetManager;
}

namespace view::widgets {

// Five text tabs (Search, Results, Player, Queue, Settings). The hardware page
// keys are forwarded as clicks only while this bar belongs to the visible
// screen, see activate()/deactivate().
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

    void activate();
    void deactivate();

private:
    void apply_styles();
    static void tab_clicked_cb(lv_event_t* event);
    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<lv_obj_t*, 5> tabs_{};
    std::array<lv_obj_t*, 5> labels_{};
    lv_observer_t* page_observer_{nullptr};
    lv_observer_t* theme_observer_{nullptr};
    bool active_{false};
};

} // namespace view::widgets
