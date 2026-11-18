/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "asset_manager.h"
#include "help_popup.h"
#include "i18n.h"
#include "keyboard_map.h"
#include "logger.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "/etc/cardtube.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "cardtube" / "cardtube.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "cardtube" / "cardtube.conf").string();
        }
    }
    return "/tmp/cardtube/cardtube.conf";
#endif
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model,
                       AssetManager& assets,
                       bool demo_mode)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)),
          demo_mode_(demo_mode) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
    }

    ~ShortcutController() {
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    bool handle_key(uint32_t key, const char* key_name, bool long_pressed, bool repeat) {
        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen();
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                    toast_->show_highlighted(i18n::tr("Saved to "),
                                             result.path,
                                             "",
                                             view::widgets::ToastTone::Success,
                                             5200,
                                             true);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                    toast_->show(std::string(i18n::tr("Screenshot failed: ")) + result.error,
                                 view::widgets::ToastTone::Error,
                                 3200);
                }
            }
            return true;
        }

        if (key == platform::kKeyHelp) {
            if (!long_pressed) {
                toast_->hide();
                if (help_popup_->visible()) {
                    help_popup_->hide();
                }
                else {
                    help_popup_->show(view_model_.current_page());
                }
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        // While the video owns the framebuffer the UI is frozen, so the only
        // useful key is ESC (close). Swallow everything else.
        if (view_model_.video_running() || view_model_.video_starting()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                view_model_.stop_video();
            }
            return true;
        }

        if (key == LV_KEY_ESC && !demo_mode_) {
            return handle_escape(long_pressed);
        }

        return view_model_.handle_key(key, key_name, long_pressed, repeat);
    }

    bool handle_escape(bool long_pressed) {
        const bool on_search = view_model_.current_page() == model::AppPage::Search;

        if (long_pressed) {
            if (exit_key_pressed_ == LV_KEY_ESC && exit_armed_) {
                toast_->hide();
                view_model_.request_quit();
            }
            return true;
        }

        if (!on_search) {
            return view_model_.handle_key(LV_KEY_ESC, "ESC", false, false);
        }

        exit_key_pressed_ = LV_KEY_ESC;
        exit_armed_ = true;
        toast_->show_persistent_highlighted(i18n::tr("Hold "),
                                            "ESC",
                                            i18n::tr(" to exit"),
                                            view::widgets::ToastTone::Warning);
        return true;
    }

    void handle_release(uint32_t key) {
        if (key != exit_key_pressed_) {
            return;
        }

        if (exit_armed_) {
            toast_->hide();
        }
        exit_key_pressed_ = 0;
        exit_armed_ = false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char* key_name,
                                    bool long_pressed,
                                    bool repeat,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, key_name, long_pressed, repeat);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    bool demo_mode_{false};
    uint32_t exit_key_pressed_{0};
    bool exit_armed_{false};
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

struct DemoPage {
    model::AppPage page;
    const char* filename;
};

constexpr DemoPage kDemoPages[] = {
    {model::AppPage::Search, "cardtube_01_search.png"},
    {model::AppPage::Results, "cardtube_02_results.png"},
    {model::AppPage::NowPlaying, "cardtube_03_player.png"},
    {model::AppPage::Queue, "cardtube_04_queue.png"},
    {model::AppPage::Settings, "cardtube_05_settings.png"},
};

void wait_for_ui(lv_display_t* display, int frames) {
    for (int index = 0; index < frames; ++index) {
        lv_timer_handler();
        lv_delay_ms(10);
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
}

void run_demo_capture(lv_display_t* display,
                      viewmodel::BaseViewModel& view_model,
                      ScreenManager& screen_manager,
                      const std::string& directory) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        LOG_WARN("demo: cannot create {}: {}", directory, filesystem_error.message());
        return;
    }

    for (const auto& demo : kDemoPages) {
        view_model.set_page(demo.page);
        screen_manager.show_page_now(demo.page);
        wait_for_ui(display, 20);

        const auto path = (std::filesystem::path(directory) / demo.filename).string();
        const auto result = platform::screenshot::capture_active_screen_to(path);
        if (result.success) {
            LOG_INFO("demo screenshot: {}", result.path);
        }
        else {
            LOG_WARN("demo screenshot failed for {}: {}", path, result.error);
        }
    }

    view_model.set_page(model::AppPage::Search);
    screen_manager.show_page_now(model::AppPage::Search);
    wait_for_ui(display, 5);
}

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("cardtube");

    std::string keyboard_error;
    if (!platform::keyboard_map_self_test(keyboard_error)) {
        LOG_ERROR("keyboard map self-test failed: {}", keyboard_error);
    }
    else {
        LOG_INFO("keyboard map self-test passed");
    }

    const bool demo_mode = std::getenv("CARDTUBE_DEMO") != nullptr;
    const char* screenshot_directory = std::getenv("CARDTUBE_SCREENSHOT_DIR");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    if (!demo_mode) {
        const std::string user_config_path = writable_config_path();
        std::string loaded_config_path = APP_CONFIG_FILE;
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
        view_model.load_config(loaded_config_path);
        view_model.set_config_path(user_config_path);
    }
    else {
        view_model.load_demo_state();
    }

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    view_model.set_ui_refresh_callback([&screen_manager]() { screen_manager.rebuild(); });
    screen_manager.start();
    ShortcutController shortcuts(view_model, assets, demo_mode);

    if (demo_mode && screenshot_directory && screenshot_directory[0] != '\0') {
        wait_for_ui(display, 10);
        run_demo_capture(display, view_model, screen_manager, screenshot_directory);
        return 0;
    }

    if (!demo_mode) {
        view_model.startup_check();
    }

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}",
             lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));

    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        // Video playback draws straight to the framebuffer: freeze LVGL, keep
        // only the global key listeners alive, and wait for mpv to exit.
        if (view_model.video_starting() || view_model.video_running()) {
            platform::pump_input();
            lv_delay_ms(10);
            continue;
        }

        // Reaps a session that ended between two polls (no-op when idle).
        view_model.finish_video();
        std::string video_error;
        view_model.take_video_error(video_error);

        lv_timer_handler();
        lv_delay_ms(5);
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }

    if (!demo_mode) {
        view_model.save_config_now();
    }

    return 0;
}

} // namespace app
