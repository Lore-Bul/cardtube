/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

class SearchScreen : public BaseScreen {
public:
    using BaseScreen::BaseScreen;

protected:
    void build_content(lv_obj_t* content) override;

private:
    void refresh_query();
    void apply_theme();

    static void query_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void theme_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void search_clicked_cb(lv_event_t* event);
    static void clear_clicked_cb(lv_event_t* event);

    lv_obj_t* field_{nullptr};
    lv_obj_t* query_label_{nullptr};
    lv_obj_t* search_button_{nullptr};
    lv_obj_t* clear_button_{nullptr};
    lv_obj_t* hint_{nullptr};
};

} // namespace screen
