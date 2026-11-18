/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class ResultsScreen : public BaseScreen {
public:
    using BaseScreen::BaseScreen;

protected:
    void build_content(lv_obj_t* content) override;

private:
    static constexpr int kVisibleRows = 3;
    static constexpr int kRowHeight = 27;
    static constexpr int kRowStep = 29;

    void refresh();

    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void row_clicked_cb(lv_event_t* event);

    struct Row {
        lv_obj_t* row{nullptr};
        lv_obj_t* thumb_holder{nullptr};
        lv_obj_t* image{nullptr};
        lv_obj_t* placeholder{nullptr};
        lv_obj_t* title{nullptr};
        lv_obj_t* meta{nullptr};
    };

    std::array<Row, kVisibleRows> rows_{};
    lv_obj_t* empty_label_{nullptr};
    lv_obj_t* empty_hint_{nullptr};
};

} // namespace screen
