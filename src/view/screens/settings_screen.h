/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class SettingsScreen : public BaseScreen {
public:
    using BaseScreen::BaseScreen;

protected:
    void build_content(lv_obj_t* content) override;

private:
    static constexpr int kRowCount = 8;
    static constexpr int kRowHeight = 22;

    void refresh();

    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void row_clicked_cb(lv_event_t* event);

    struct Row {
        lv_obj_t* row{nullptr};
        lv_obj_t* name{nullptr};
        lv_obj_t* value{nullptr};
    };

    lv_obj_t* list_{nullptr};
    std::array<Row, kRowCount> rows_{};
};

} // namespace screen
