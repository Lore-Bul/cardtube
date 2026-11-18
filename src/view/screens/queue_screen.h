/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class QueueScreen : public BaseScreen {
public:
    using BaseScreen::BaseScreen;

protected:
    void build_content(lv_obj_t* content) override;

private:
    static constexpr int kVisibleRows = 3;
    static constexpr int kRowHeight = 23;

    void refresh();

    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void row_clicked_cb(lv_event_t* event);

    struct Row {
        lv_obj_t* row{nullptr};
        lv_obj_t* index{nullptr};
        lv_obj_t* title{nullptr};
        lv_obj_t* duration{nullptr};
    };

    std::array<Row, kVisibleRows> rows_{};
    lv_obj_t* header_{nullptr};
    lv_obj_t* empty_label_{nullptr};
    lv_obj_t* empty_hint_{nullptr};
};

} // namespace screen
