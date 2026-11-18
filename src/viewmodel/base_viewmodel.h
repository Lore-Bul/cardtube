/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"
#include "i18n.h"
#include "linux_input.h"
#include "subjects.h"

#include "lvgl.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace viewmodel {

class BaseViewModel {
public:
    static constexpr int kSettingCount = 10;

    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    // ---- labels and state exposed to the views -------------------------
    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* query_subject();
    lv_subject_t* status_subject();

    lv_subject_t* results_version_subject();
    lv_subject_t* results_count_subject();
    lv_subject_t* selected_result_subject();
    lv_subject_t* thumb_version_subject();

    lv_subject_t* queue_version_subject();
    lv_subject_t* queue_count_subject();
    lv_subject_t* queue_selection_subject();
    lv_subject_t* queue_index_subject();

    lv_subject_t* now_playing_version_subject();
    lv_subject_t* now_playing_title_subject();
    lv_subject_t* now_playing_channel_subject();
    lv_subject_t* playing_subject();
    lv_subject_t* paused_subject();
    lv_subject_t* position_subject();
    lv_subject_t* duration_subject();
    lv_subject_t* progress_subject();

    lv_subject_t* volume_subject();
    lv_subject_t* quality_subject();
    lv_subject_t* video_mode_subject();
    lv_subject_t* autoplay_subject();
    lv_subject_t* settings_selection_subject();
    lv_subject_t* settings_version_subject();

    lv_subject_t* quit_requested_subject();

    // ---- plain data access ---------------------------------------------
    model::BaseModel& model();
    const model::BaseModel& model() const;

    const std::vector<model::TrackInfo>& results() const;
    const std::vector<model::TrackInfo>& queue() const;
    const model::TrackInfo& now_playing() const;
    int selected_result() const;
    int queue_selection() const;
    int queue_index() const;
    int settings_selection() const;

    model::AppPage current_page() const;
    bool is_dark_mode() const;
    bool is_searching() const;
    bool is_playing() const;
    bool is_paused() const;
    bool nav_shortcut_allowed() const;
    const std::string& config_path() const;

    i18n::Language language() const;
    platform::KeyboardLayout keyboard_layout() const;

    // Rebuilds the LVGL screens so new translations are picked up. Registered
    // by the app once the ScreenManager exists.
    void set_ui_refresh_callback(std::function<void()> callback);

    std::string setting_label(int index) const;
    std::string setting_value(int index) const;

    // ---- navigation ------------------------------------------------------
    void set_page(model::AppPage page);
    void cycle_page(int delta);
    void toggle_dark_mode();
    void set_dark_mode(bool enabled);
    void request_quit();

    // ---- search ----------------------------------------------------------
    void append_query_char(char character);
    void append_query_codepoint(uint32_t codepoint);
    void append_query(const std::string& text);
    void backspace_query();
    void clear_query();
    void start_search();
    void open_search_page();

    // ---- results ---------------------------------------------------------
    void move_result_selection(int delta);
    void enqueue_selected();
    void enqueue_track(const model::TrackInfo& track);
    void play_result(int index);
    void play_result_video(int index);
    void play_selected();

    // ---- queue -----------------------------------------------------------
    void move_queue_selection(int delta);
    void play_queue_index(int index);
    void remove_queue_selection();
    void clear_queue();
    void play_next();
    void play_previous();

    // ---- playback --------------------------------------------------------
    void toggle_pause();
    void seek_relative(double seconds);
    void stop_playback();
    void change_volume(int delta);
    void set_volume(int percent);
    void play_now_as_video();

    // ---- settings --------------------------------------------------------
    void move_settings_selection(int delta);
    void select_settings(int index);
    void adjust_setting(int delta);
    void activate_setting();
    void check_dependencies();
    void update_ytdlp();
    void show_version_info();
    void set_language(i18n::Language language);
    void set_keyboard_layout(platform::KeyboardLayout layout);

    // ---- keyboard --------------------------------------------------------
    bool handle_key(uint32_t key, const char* key_name, bool long_pressed, bool repeat);

    // ---- configuration ---------------------------------------------------
    void load_config(const std::string& path);
    void set_config_path(const std::string& path);
    void save_config_now();

    // ---- video session ---------------------------------------------------
    // True while the video launcher is still starting mpv.
    bool video_starting();
    // True while the framebuffer mpv process owns the display.
    bool video_running();
    // Called after video_running() turned false; restores the UI state.
    void finish_video();
    void stop_video();
    bool take_video_error(std::string& error);

    // Runs a dependency probe once after startup.
    void startup_check();

    // Fills the model with believable content for desktop demos and for the
    // store screenshots (CARDTUBE_DEMO=1).
    void load_demo_state();

private:
    void publish_all();
    void publish_results();
    void publish_queue();
    void publish_now_playing();
    void publish_settings();

    void set_status(const std::string& message);
    void set_title();
    void append_query_bytes(const std::string& bytes);
    void persist();
    void schedule_save();
    void apply_volume_to_player();
    void prefetch_thumbnails(const std::vector<model::TrackInfo>& tracks);
    void prefetch_thumbnail(const std::string& video_id);

    void start_audio_playback(const model::TrackInfo& track);
    void start_video_playback(const model::TrackInfo& track);
    void handle_auto_advance(double position_s);

    static void monitor_timer_cb(lv_timer_t* timer);
    static void save_timer_cb(lv_timer_t* timer);

    model::BaseModel model_;

    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::StringSubject<120> query_subject_;
    reactive::StringSubject<160> status_subject_;

    reactive::IntSubject results_version_subject_;
    reactive::IntSubject results_count_subject_;
    reactive::IntSubject selected_result_subject_;
    reactive::IntSubject thumb_version_subject_;

    reactive::IntSubject queue_version_subject_;
    reactive::IntSubject queue_count_subject_;
    reactive::IntSubject queue_selection_subject_;
    reactive::IntSubject queue_index_subject_;

    reactive::IntSubject now_playing_version_subject_;
    reactive::StringSubject<160> now_playing_title_subject_;
    reactive::StringSubject<96> now_playing_channel_subject_;
    reactive::BoolSubject playing_subject_;
    reactive::BoolSubject paused_subject_;
    reactive::IntSubject position_subject_;
    reactive::IntSubject duration_subject_;
    reactive::IntSubject progress_subject_;

    reactive::IntSubject volume_subject_;
    reactive::StringSubject<12> quality_subject_;
    reactive::BoolSubject video_mode_subject_;
    reactive::BoolSubject autoplay_subject_;
    reactive::IntSubject settings_selection_subject_;
    reactive::IntSubject settings_version_subject_;

    reactive::BoolSubject quit_requested_subject_;

    lv_timer_t* monitor_timer_{nullptr};
    lv_timer_t* save_timer_{nullptr};

    std::string config_path_;
    std::string status_text_;
    bool search_inflight_{false};
    bool audio_inflight_{false};
    bool dependencies_checked_{false};

    i18n::Language language_{i18n::Language::English};
    platform::KeyboardLayout keyboard_layout_{platform::KeyboardLayout::Us};
    std::function<void()> ui_refresh_callback_;

    std::shared_ptr<std::atomic<bool>> alive_;

    // Video session state. The one-shot mpv process runs outside the LVGL
    // refresh loop, so the main loop only starts pumping the framebuffer once
    // the background launcher reports success.
    std::atomic<bool> video_attempt_{false};
    std::atomic<bool> video_ready_{false};
    std::atomic<bool> video_failed_{false};
    std::atomic<bool> video_cancel_{false};
    std::mutex video_error_mutex_;
    std::string video_error_;
};

} // namespace viewmodel
