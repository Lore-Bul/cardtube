/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

class NowPlayingScreen : public BaseScreen {
public:
    using BaseScreen::BaseScreen;

protected:
    void build_content(lv_obj_t* content) override;

private:
    void refresh();

    static void update_cb(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* thumb_holder_{nullptr};
    lv_obj_t* image_{nullptr};
    lv_obj_t* placeholder_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* channel_{nullptr};
    lv_obj_t* state_{nullptr};
    lv_obj_t* progress_{nullptr};
    lv_obj_t* time_{nullptr};
    lv_obj_t* hints_{nullptr};
};

} // namespace screen
