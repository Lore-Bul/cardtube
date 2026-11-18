/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "application_config.h"
#include "logger.h"
#include "mpv_player.h"
#include "mpv_video.h"
#include "process.h"
#include "ytdlp.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace viewmodel {
namespace {

constexpr int kPageCount = 5;
constexpr int kSearchLimit = 12;
constexpr double kSeekStepSeconds = 10.0;

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

const char* page_name(model::AppPage page) {
    switch (page) {
        case model::AppPage::Search:
            return i18n::tr("Search");
        case model::AppPage::Results:
            return i18n::tr("Results");
        case model::AppPage::NowPlaying:
            return i18n::tr("Player");
        case model::AppPage::Queue:
            return i18n::tr("Queue");
        case model::AppPage::Settings:
            return i18n::tr("Settings");
    }
    return "CardTube";
}

const char* page_hint(model::AppPage page) {
    switch (page) {
        case model::AppPage::Search:
            return i18n::tr("Type a search and press Enter");
        case model::AppPage::Results:
            return i18n::tr("UP/DOWN select - Enter plays - Q queues - V video");
        case model::AppPage::NowPlaying:
            return i18n::tr("SPACE pause - LEFT/RIGHT seek - N next - V video");
        case model::AppPage::Queue:
            return i18n::tr("UP/DOWN select - Enter plays - D removes - C clears");
        case model::AppPage::Settings:
            return i18n::tr("UP/DOWN select - LEFT/RIGHT changes - Enter confirms");
    }
    return "CardTube";
}

int clamp_index(int index, int size) {
    if (size <= 0) {
        return 0;
    }
    return std::clamp(index, 0, size - 1);
}

int find_track(const std::vector<model::TrackInfo>& tracks, const std::string& id) {
    const auto it = std::find_if(tracks.begin(),
                                 tracks.end(),
                                 [&id](const model::TrackInfo& track) {
                                     return track.id == id;
                                 });
    if (it == tracks.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(tracks.begin(), it));
}

std::string quality_display(const std::string& quality) {
    return quality == "best" ? std::string(i18n::tr("Best")) : quality;
}

const std::string& next_quality(const std::string& current) {
    static const std::string kBest = "best";
    static const std::string k480 = "480p";
    static const std::string k360 = "360p";
    if (current == "best") {
        return k480;
    }
    if (current == "480p") {
        return k360;
    }
    return kBest;
}

const std::string& previous_quality(const std::string& current) {
    static const std::string kBest = "best";
    static const std::string k480 = "480p";
    static const std::string k360 = "360p";
    if (current == "360p") {
        return k480;
    }
    if (current == "480p") {
        return kBest;
    }
    return k360;
}

const char* kSettingLabels[BaseViewModel::kSettingCount] = {
    "Theme",
    "Language",
    "Keyboard",
    "Video mode",
    "Autoplay next",
    "Volume",
    "Quality",
    "Dependencies",
    "Update yt-dlp",
    "Quit CardTube",
};

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_("CardTube"),
      dark_mode_subject_(true),
      current_page_subject_(page_to_int(model::AppPage::Search)),
      query_subject_(""),
      status_subject_(i18n::tr("Type a search and press Enter")),
      results_version_subject_(0),
      results_count_subject_(0),
      selected_result_subject_(0),
      thumb_version_subject_(0),
      queue_version_subject_(0),
      queue_count_subject_(0),
      queue_selection_subject_(0),
      queue_index_subject_(-1),
      now_playing_version_subject_(0),
      now_playing_title_subject_(i18n::tr("Nothing is playing yet")),
      now_playing_channel_subject_(""),
      playing_subject_(false),
      paused_subject_(false),
      position_subject_(0),
      duration_subject_(0),
      progress_subject_(0),
      volume_subject_(model_.volume()),
      quality_subject_(model_.quality().c_str()),
      video_mode_subject_(model_.video_mode()),
      autoplay_subject_(model_.autoplay_next()),
      settings_selection_subject_(0),
      settings_version_subject_(0),
      quit_requested_subject_(false) {
    alive_ = std::make_shared<std::atomic<bool>>(true);
    status_text_ = i18n::tr("Type a search and press Enter");

    monitor_timer_ = lv_timer_create(monitor_timer_cb, 500, this);
    save_timer_ = lv_timer_create(save_timer_cb, 900, this);
    if (save_timer_) {
        lv_timer_set_repeat_count(save_timer_, 1);
        lv_timer_pause(save_timer_);
    }

    publish_all();
}

BaseViewModel::~BaseViewModel() {
    alive_->store(false);

    if (monitor_timer_) {
        lv_timer_delete(monitor_timer_);
        monitor_timer_ = nullptr;
    }
    if (save_timer_) {
        lv_timer_delete(save_timer_);
        save_timer_ = nullptr;
    }

    if (video_attempt_.load() || video_ready_.load()) {
        platform::video::stop();
        video_attempt_.store(false);
        video_ready_.store(false);
    }
    platform::mpv::stop();
}

// ---------------------------------------------------------------------------
// Subjects
// ---------------------------------------------------------------------------

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::query_subject() {
    return query_subject_.native();
}

lv_subject_t* BaseViewModel::status_subject() {
    return status_subject_.native();
}

lv_subject_t* BaseViewModel::results_version_subject() {
    return results_version_subject_.native();
}

lv_subject_t* BaseViewModel::results_count_subject() {
    return results_count_subject_.native();
}

lv_subject_t* BaseViewModel::selected_result_subject() {
    return selected_result_subject_.native();
}

lv_subject_t* BaseViewModel::thumb_version_subject() {
    return thumb_version_subject_.native();
}

lv_subject_t* BaseViewModel::queue_version_subject() {
    return queue_version_subject_.native();
}

lv_subject_t* BaseViewModel::queue_count_subject() {
    return queue_count_subject_.native();
}

lv_subject_t* BaseViewModel::queue_selection_subject() {
    return queue_selection_subject_.native();
}

lv_subject_t* BaseViewModel::queue_index_subject() {
    return queue_index_subject_.native();
}

lv_subject_t* BaseViewModel::now_playing_version_subject() {
    return now_playing_version_subject_.native();
}

lv_subject_t* BaseViewModel::now_playing_title_subject() {
    return now_playing_title_subject_.native();
}

lv_subject_t* BaseViewModel::now_playing_channel_subject() {
    return now_playing_channel_subject_.native();
}

lv_subject_t* BaseViewModel::playing_subject() {
    return playing_subject_.native();
}

lv_subject_t* BaseViewModel::paused_subject() {
    return paused_subject_.native();
}

lv_subject_t* BaseViewModel::position_subject() {
    return position_subject_.native();
}

lv_subject_t* BaseViewModel::duration_subject() {
    return duration_subject_.native();
}

lv_subject_t* BaseViewModel::progress_subject() {
    return progress_subject_.native();
}

lv_subject_t* BaseViewModel::volume_subject() {
    return volume_subject_.native();
}

lv_subject_t* BaseViewModel::quality_subject() {
    return quality_subject_.native();
}

lv_subject_t* BaseViewModel::video_mode_subject() {
    return video_mode_subject_.native();
}

lv_subject_t* BaseViewModel::autoplay_subject() {
    return autoplay_subject_.native();
}

lv_subject_t* BaseViewModel::settings_selection_subject() {
    return settings_selection_subject_.native();
}

lv_subject_t* BaseViewModel::settings_version_subject() {
    return settings_version_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

model::BaseModel& BaseViewModel::model() {
    return model_;
}

const model::BaseModel& BaseViewModel::model() const {
    return model_;
}

const std::vector<model::TrackInfo>& BaseViewModel::results() const {
    return model_.results();
}

const std::vector<model::TrackInfo>& BaseViewModel::queue() const {
    return model_.queue();
}

const model::TrackInfo& BaseViewModel::now_playing() const {
    return model_.now_playing();
}

int BaseViewModel::selected_result() const {
    return model_.selected_result();
}

int BaseViewModel::queue_selection() const {
    return model_.queue_selection();
}

int BaseViewModel::queue_index() const {
    return model_.queue_index();
}

int BaseViewModel::settings_selection() const {
    return settings_selection_subject_.value();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

bool BaseViewModel::is_searching() const {
    return search_inflight_;
}

bool BaseViewModel::is_playing() const {
    return playing_subject_.value();
}

bool BaseViewModel::is_paused() const {
    return paused_subject_.value();
}

bool BaseViewModel::nav_shortcut_allowed() const {
    return model_.current_page() != model::AppPage::Search;
}

const std::string& BaseViewModel::config_path() const {
    return config_path_;
}

std::string BaseViewModel::setting_label(int index) const {
    if (index < 0 || index >= kSettingCount) {
        return {};
    }
    return i18n::tr(kSettingLabels[index]);
}

std::string BaseViewModel::setting_value(int index) const {
    switch (index) {
        case 0:
            return i18n::tr(model_.dark_mode() ? "Dark" : "Light");
        case 1:
            return i18n::language_name(language_);
        case 2:
            return keyboard_layout_ == platform::KeyboardLayout::It ? "IT" : "US";
        case 3:
            return i18n::tr(model_.video_mode() ? "On" : "Off");
        case 4:
            return i18n::tr(model_.autoplay_next() ? "On" : "Off");
        case 5:
            return std::to_string(model_.volume()) + "%";
        case 6:
            return quality_display(model_.quality());
        case 7:
            if (!dependencies_checked_) {
                return i18n::tr("Press Enter");
            }
            return platform::ytdlp::dependencies().ready() ? i18n::tr("Ready")
                                                         : i18n::tr("Missing tools");
        case 8:
            return i18n::tr("Press Enter");
        case 9:
            return i18n::tr("Press Enter");
        default:
            return {};
    }
}

i18n::Language BaseViewModel::language() const {
    return language_;
}

platform::KeyboardLayout BaseViewModel::keyboard_layout() const {
    return keyboard_layout_;
}

void BaseViewModel::set_ui_refresh_callback(std::function<void()> callback) {
    ui_refresh_callback_ = std::move(callback);
}

// ---------------------------------------------------------------------------
// Publishing helpers
// ---------------------------------------------------------------------------

void BaseViewModel::publish_all() {
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
    query_subject_.set(model_.query().c_str());
    status_subject_.set(status_text_.c_str());
    volume_subject_.set(model_.volume());
    quality_subject_.set(model_.quality().c_str());
    video_mode_subject_.set(model_.video_mode());
    autoplay_subject_.set(model_.autoplay_next());
    set_title();
    publish_results();
    publish_queue();
    publish_now_playing();
    publish_settings();
}

void BaseViewModel::publish_results() {
    results_count_subject_.set(static_cast<int>(model_.results().size()));
    results_version_subject_.set(results_version_subject_.value() + 1);
    selected_result_subject_.set(model_.selected_result());
}

void BaseViewModel::publish_queue() {
    queue_count_subject_.set(static_cast<int>(model_.queue().size()));
    queue_version_subject_.set(queue_version_subject_.value() + 1);
    queue_selection_subject_.set(model_.queue_selection());
    queue_index_subject_.set(model_.queue_index());
}

void BaseViewModel::publish_now_playing() {
    const auto& track = model_.now_playing();
    now_playing_title_subject_.set(track.valid() ? track.title.c_str()
                                                 : i18n::tr("Nothing is playing yet"));
    now_playing_channel_subject_.set(track.valid() ? track.channel.c_str() : "");
    now_playing_version_subject_.set(now_playing_version_subject_.value() + 1);
}

void BaseViewModel::publish_settings() {
    settings_selection_subject_.set(clamp_index(settings_selection_subject_.value(), kSettingCount));
    settings_version_subject_.set(settings_version_subject_.value() + 1);
}

void BaseViewModel::set_status(const std::string& message) {
    status_text_ = message;
    status_subject_.set(status_text_.c_str());
    LOG_INFO("status: {}", status_text_);
}

void BaseViewModel::set_title() {
    std::string title = "CardTube - ";
    title += page_name(model_.current_page());
    title_subject_.set(title.c_str());
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void BaseViewModel::set_page(model::AppPage page) {
    if (model_.current_page() == page) {
        return;
    }
    model_.set_current_page(page);
    current_page_subject_.set(page_to_int(page));
    set_title();
    set_status(page_hint(page));
}

void BaseViewModel::cycle_page(int delta) {
    if (delta == 0) {
        return;
    }
    int index = page_to_int(model_.current_page()) + delta;
    index %= kPageCount;
    if (index < 0) {
        index += kPageCount;
    }
    set_page(static_cast<model::AppPage>(index));
}

void BaseViewModel::toggle_dark_mode() {
    set_dark_mode(!model_.dark_mode());
}

void BaseViewModel::set_dark_mode(bool enabled) {
    if (model_.dark_mode() == enabled) {
        return;
    }
    model_.set_dark_mode(enabled);
    dark_mode_subject_.set(enabled);
    publish_settings();
    persist();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void BaseViewModel::append_query_bytes(const std::string& bytes) {
    if (bytes.empty()) {
        return;
    }
    std::string query = model_.query();
    if (query.size() + bytes.size() > 110) {
        return;
    }
    query += bytes;
    model_.set_query(std::move(query));
    query_subject_.set(model_.query().c_str());
}

void BaseViewModel::append_query_char(char character) {
    const auto unsigned_character = static_cast<unsigned char>(character);
    if (unsigned_character < 32 || unsigned_character == 127) {
        return;
    }
    append_query_bytes(std::string(1, character));
}

void BaseViewModel::append_query_codepoint(uint32_t codepoint) {
    if (codepoint < 32 || codepoint == 127 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return;
    }

    std::string bytes;
    if (codepoint < 0x80) {
        bytes.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint < 0x800) {
        bytes.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        bytes.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000) {
        bytes.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        bytes.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        bytes.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else {
        bytes.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        bytes.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        bytes.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        bytes.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    append_query_bytes(bytes);
}

void BaseViewModel::append_query(const std::string& text) {
    append_query_bytes(text);
}

void BaseViewModel::backspace_query() {
    if (model_.query().empty()) {
        return;
    }
    std::string query = model_.query();
    while (!query.empty() && (static_cast<unsigned char>(query.back()) & 0xC0) == 0x80) {
        query.pop_back();
    }
    if (!query.empty()) {
        query.pop_back();
    }
    model_.set_query(std::move(query));
    query_subject_.set(model_.query().c_str());
}

void BaseViewModel::clear_query() {
    model_.set_query({});
    query_subject_.set("");
}

void BaseViewModel::open_search_page() {
    set_page(model::AppPage::Search);
}

void BaseViewModel::start_search() {
    if (search_inflight_) {
        set_status(i18n::tr("Search already in progress"));
        return;
    }

    const std::string query = platform::trim_copy(model_.query());
    if (query.empty()) {
        set_status(i18n::tr("Type something to search"));
        return;
    }

    search_inflight_ = true;
    set_status(std::string(i18n::tr("Searching YouTube for ")) + "\"" + query + "\"...");

    auto alive = alive_;
    platform::submit_background([this, query, alive]() {
        const auto result = platform::ytdlp::search(query, kSearchLimit);
        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, result, alive]() {
            if (!alive->load()) {
                return;
            }
            search_inflight_ = false;

            if (!result.ok) {
                set_status(std::string(i18n::tr("Search failed: ")) + result.error);
                LOG_WARN("search failed: {}", result.error);
                return;
            }

            model_.results() = result.tracks;
            model_.set_selected_result(0);
            publish_results();
            set_page(model::AppPage::Results);
            set_status(std::to_string(model_.results().size()) +
                       i18n::tr(" results - Enter plays, Q queues, V video"));
            prefetch_thumbnails(model_.results());
        });
    });
}

void BaseViewModel::prefetch_thumbnails(const std::vector<model::TrackInfo>& tracks) {
    const auto copy = tracks;
    auto alive = alive_;
    platform::submit_background([this, copy, alive]() {
        for (const auto& track : copy) {
            if (!alive->load()) {
                return;
            }
            if (platform::ytdlp::thumbnail_cached(track.id)) {
                continue;
            }
            platform::ytdlp::fetch_thumbnail(track.id, platform::ytdlp::thumbnail_path(track.id));
        }

        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, alive]() {
            if (!alive->load()) {
                return;
            }
            thumb_version_subject_.set(thumb_version_subject_.value() + 1);
        });
    });
}

void BaseViewModel::prefetch_thumbnail(const std::string& video_id) {
    if (video_id.empty() || platform::ytdlp::thumbnail_cached(video_id)) {
        return;
    }
    auto alive = alive_;
    platform::submit_background([this, video_id, alive]() {
        if (!alive->load()) {
            return;
        }
        const bool fetched =
            platform::ytdlp::fetch_thumbnail(video_id, platform::ytdlp::thumbnail_path(video_id));
        if (!fetched || !alive->load()) {
            return;
        }
        platform::post_to_ui([this, alive]() {
            if (alive->load()) {
                thumb_version_subject_.set(thumb_version_subject_.value() + 1);
            }
        });
    });
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

void BaseViewModel::move_result_selection(int delta) {
    if (model_.results().empty()) {
        return;
    }
    model_.set_selected_result(model_.selected_result() + delta);
    selected_result_subject_.set(model_.selected_result());
}

void BaseViewModel::enqueue_track(const model::TrackInfo& track) {
    if (!track.valid()) {
        return;
    }

    auto& queue = model_.queue();
    if (find_track(queue, track.id) >= 0) {
        set_status(std::string(i18n::tr("Already in the queue: ")) + track.title);
        return;
    }

    queue.push_back(track);
    publish_queue();
    set_status(std::string(i18n::tr("Queued: ")) + track.title);
}

void BaseViewModel::enqueue_selected() {
    const auto* track = model_.selected_track();
    if (!track) {
        set_status(i18n::tr("No result selected - run a search first"));
        return;
    }
    enqueue_track(*track);
}

void BaseViewModel::play_result(int index) {
    if (index < 0 || index >= static_cast<int>(model_.results().size())) {
        return;
    }

    model_.set_selected_result(index);
    selected_result_subject_.set(index);

    const model::TrackInfo track = model_.results()[static_cast<std::size_t>(index)];
    enqueue_track(track);

    if (model_.video_mode()) {
        start_video_playback(track);
        return;
    }

    const int queue_position = find_track(model_.queue(), track.id);
    if (queue_position >= 0) {
        play_queue_index(queue_position);
    }
}

void BaseViewModel::play_result_video(int index) {
    if (index < 0 || index >= static_cast<int>(model_.results().size())) {
        set_status(i18n::tr("No result selected"));
        return;
    }
    model_.set_selected_result(index);
    selected_result_subject_.set(index);
    start_video_playback(model_.results()[static_cast<std::size_t>(index)]);
}

void BaseViewModel::play_selected() {
    play_result(model_.selected_result());
}

// ---------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------

void BaseViewModel::move_queue_selection(int delta) {
    if (model_.queue().empty()) {
        return;
    }
    model_.set_queue_selection(model_.queue_selection() + delta);
    queue_selection_subject_.set(model_.queue_selection());
}

void BaseViewModel::play_queue_index(int index) {
    if (index < 0 || index >= static_cast<int>(model_.queue().size())) {
        set_status(i18n::tr("Nothing to play in the queue"));
        return;
    }

    model_.set_queue_index(index);
    queue_index_subject_.set(index);

    const model::TrackInfo track = model_.queue()[static_cast<std::size_t>(index)];
    model_.set_now_playing(track);
    publish_now_playing();
    prefetch_thumbnail(track.id);
    set_page(model::AppPage::NowPlaying);

    if (model_.video_mode()) {
        start_video_playback(track);
        return;
    }

    start_audio_playback(track);
}

void BaseViewModel::remove_queue_selection() {
    auto& queue = model_.queue();
    if (queue.empty()) {
        set_status(i18n::tr("The queue is empty"));
        return;
    }

    const int index = model_.queue_selection();
    const std::string title = queue[static_cast<std::size_t>(index)].title;
    queue.erase(queue.begin() + index);

    if (model_.queue_index() > index) {
        model_.set_queue_index(model_.queue_index() - 1);
    }
    else if (model_.queue_index() == index) {
        model_.set_queue_index(index);
    }

    model_.set_queue_selection(std::min(index, static_cast<int>(queue.size()) - 1));
    publish_queue();
    set_status(std::string(i18n::tr("Removed: ")) + title);
}

void BaseViewModel::clear_queue() {
    if (model_.queue().empty()) {
        set_status(i18n::tr("The queue is already empty"));
        return;
    }
    model_.queue().clear();
    model_.set_queue_index(-1);
    model_.set_queue_selection(0);
    publish_queue();
    set_status(i18n::tr("Queue cleared"));
}

void BaseViewModel::play_next() {
    const auto& queue = model_.queue();
    const int next = model_.queue_index() + 1;
    if (next < static_cast<int>(queue.size())) {
        play_queue_index(next);
        return;
    }
    set_status(i18n::tr("End of the queue"));
}

void BaseViewModel::play_previous() {
    const int previous = model_.queue_index() - 1;
    if (previous >= 0) {
        play_queue_index(previous);
        return;
    }
    if (platform::mpv::running()) {
        platform::mpv::seek_relative(-1.0e9);
        set_status(i18n::tr("Restarted the current track"));
    }
    else {
        set_status(i18n::tr("No previous track"));
    }
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void BaseViewModel::start_audio_playback(const model::TrackInfo& track) {
    if (!track.valid()) {
        set_status(i18n::tr("Invalid track"));
        return;
    }
    if (audio_inflight_) {
        set_status(i18n::tr("A track is already loading, try again in a moment"));
        return;
    }

    audio_inflight_ = true;
    set_status(std::string(i18n::tr("Loading ")) + "\"" + track.title + "\"...");

    const std::string url = track.url();
    const std::string quality = model_.quality();
    const int volume = model_.volume();
    auto alive = alive_;

    platform::submit_background([this, url, quality, volume, alive]() {
        std::string error;
        bool ok = true;

        if (!platform::mpv::running()) {
            if (!platform::mpv::start(volume)) {
                ok = false;
                error = i18n::tr("mpv is not installed or failed to start");
            }
        }
        else {
            platform::mpv::set_volume(volume);
        }

        if (ok && !platform::mpv::load(url, quality)) {
            ok = false;
            error = i18n::tr("unable to open the audio stream (see /tmp/cardtube/mpv.log)");
        }

        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, ok, error, alive]() {
            if (!alive->load()) {
                return;
            }
            audio_inflight_ = false;
            if (!ok) {
                set_status(error);
                LOG_WARN("audio playback failed: {}", error);
                return;
            }
            set_status(i18n::tr("Playing - Space pause, N next, V video"));
        });
    });
}

void BaseViewModel::start_video_playback(const model::TrackInfo& track) {
    if (!track.valid()) {
        set_status(i18n::tr("Nothing to watch"));
        return;
    }
    if (video_attempt_.load() || video_ready_.load()) {
        set_status(i18n::tr("A video is already starting or playing"));
        return;
    }
    if (!platform::command_exists("mpv")) {
        set_status(i18n::tr("mpv is not installed - video playback needs mpv"));
        return;
    }

    // Remember it as the current track even when it was not in the queue.
    const int queue_position = find_track(model_.queue(), track.id);
    if (queue_position >= 0) {
        model_.set_queue_index(queue_position);
        queue_index_subject_.set(queue_position);
    }
    model_.set_now_playing(track);
    publish_now_playing();
    set_page(model::AppPage::NowPlaying);

    // Do not let the audio daemon talk over the video.
    if (platform::mpv::running()) {
        platform::mpv::set_pause(true);
    }

    {
        std::lock_guard<std::mutex> lock(video_error_mutex_);
        video_error_.clear();
    }
    video_failed_.store(false);
    video_ready_.store(false);
    video_cancel_.store(false);
    video_attempt_.store(true);
    set_status(i18n::tr("Starting video... (ESC closes)"));

    const std::string url = track.url();
    const std::string quality = model_.quality();
    const int volume = model_.volume();
    auto alive = alive_;

    platform::submit_background([this, url, quality, volume, alive]() {
        std::string error;
        const bool ok = platform::video::play(url, quality, volume, error);
        if (!alive->load()) {
            return;
        }
        if (video_cancel_.exchange(false)) {
            platform::video::stop();
            video_attempt_.store(false);
            platform::post_to_ui([this, alive]() {
                if (alive->load()) {
                    set_status(i18n::tr("Video cancelled"));
                    lv_obj_invalidate(lv_screen_active());
                }
            });
            return;
        }
        if (ok) {
            video_ready_.store(true);
        }
        else {
            std::lock_guard<std::mutex> lock(video_error_mutex_);
            video_error_ = error;
            video_failed_.store(true);
        }
        video_attempt_.store(false);
    });
}

void BaseViewModel::toggle_pause() {
    if (!platform::mpv::running()) {
        set_status(i18n::tr("Nothing is playing"));
        return;
    }
    platform::mpv::toggle_pause();
    const bool now_paused = !paused_subject_.value();
    paused_subject_.set(now_paused);
    playing_subject_.set(!now_paused);
    set_status(now_paused ? i18n::tr("Paused") : i18n::tr("Playing"));
}

void BaseViewModel::seek_relative(double seconds) {
    if (!platform::mpv::running() || !model_.now_playing().valid()) {
        set_status(i18n::tr("Nothing is playing"));
        return;
    }
    platform::mpv::seek_relative(seconds);
    set_status(seconds < 0.0 ? i18n::tr("Seek -10s") : i18n::tr("Seek +10s"));
}

void BaseViewModel::stop_playback() {
    if (!platform::mpv::running()) {
        set_status(i18n::tr("Nothing is playing"));
        return;
    }
    platform::mpv::stop_playback();
    playing_subject_.set(false);
    paused_subject_.set(false);
    position_subject_.set(0);
    progress_subject_.set(0);
    set_status(i18n::tr("Playback stopped"));
}

void BaseViewModel::change_volume(int delta) {
    set_volume(model_.volume() + delta * 5);
}

void BaseViewModel::set_volume(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (model_.volume() == percent) {
        return;
    }
    model_.set_volume(percent);
    volume_subject_.set(percent);
    platform::mpv::set_volume(percent);
    publish_settings();
    schedule_save();
    if (model_.current_page() == model::AppPage::Settings ||
        model_.current_page() == model::AppPage::NowPlaying) {
        set_status(std::string(i18n::tr("Volume ")) + std::to_string(percent) + "%");
    }
}

void BaseViewModel::play_now_as_video() {
    if (!model_.now_playing().valid()) {
        set_status(i18n::tr("Nothing is playing - pick a result first"));
        return;
    }
    start_video_playback(model_.now_playing());
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void BaseViewModel::move_settings_selection(int delta) {
    const int next = clamp_index(settings_selection_subject_.value() + delta, kSettingCount);
    settings_selection_subject_.set(next);
}

void BaseViewModel::select_settings(int index) {
    if (index < 0 || index >= kSettingCount) {
        return;
    }
    settings_selection_subject_.set(index);
    publish_settings();
}

void BaseViewModel::adjust_setting(int delta) {
    if (delta == 0) {
        return;
    }
    const int index = settings_selection_subject_.value();
    switch (index) {
        case 0:
            set_dark_mode(delta > 0 ? true : false);
            break;
        case 1:
            set_language(language_ == i18n::Language::English ? i18n::Language::Italian
                                                              : i18n::Language::English);
            return;
        case 2:
            set_keyboard_layout(keyboard_layout_ == platform::KeyboardLayout::Us
                                    ? platform::KeyboardLayout::It
                                    : platform::KeyboardLayout::Us);
            return;
        case 3:
            model_.set_video_mode(delta > 0);
            video_mode_subject_.set(model_.video_mode());
            publish_settings();
            persist();
            set_status(model_.video_mode() ? i18n::tr("Video mode on - playback opens the video")
                                           : i18n::tr("Video mode off - audio only"));
            return;
        case 4:
            model_.set_autoplay_next(delta > 0);
            autoplay_subject_.set(model_.autoplay_next());
            publish_settings();
            persist();
            set_status(model_.autoplay_next() ? i18n::tr("Autoplay next: on")
                                              : i18n::tr("Autoplay next: off"));
            return;
        case 5:
            change_volume(delta);
            return;
        case 6: {
            const std::string& next =
                delta > 0 ? next_quality(model_.quality()) : previous_quality(model_.quality());
            model_.set_quality(next);
            quality_subject_.set(model_.quality().c_str());
            publish_settings();
            persist();
            set_status(std::string(i18n::tr("Quality: ")) + quality_display(model_.quality()));
            return;
        }
        default:
            break;
    }
    publish_settings();
}

void BaseViewModel::activate_setting() {
    switch (settings_selection_subject_.value()) {
        case 0:
            toggle_dark_mode();
            break;
        case 1:
            set_language(language_ == i18n::Language::English ? i18n::Language::Italian
                                                              : i18n::Language::English);
            break;
        case 2:
            set_keyboard_layout(keyboard_layout_ == platform::KeyboardLayout::Us
                                    ? platform::KeyboardLayout::It
                                    : platform::KeyboardLayout::Us);
            break;
        case 3:
            model_.set_video_mode(!model_.video_mode());
            video_mode_subject_.set(model_.video_mode());
            publish_settings();
            persist();
            break;
        case 4:
            model_.set_autoplay_next(!model_.autoplay_next());
            autoplay_subject_.set(model_.autoplay_next());
            publish_settings();
            persist();
            break;
        case 5:
            change_volume(1);
            break;
        case 6:
            adjust_setting(1);
            break;
        case 7:
            check_dependencies();
            break;
        case 8:
            update_ytdlp();
            break;
        case 9:
            request_quit();
            break;
        default:
            break;
    }
}

void BaseViewModel::set_language(i18n::Language language) {
    if (language_ == language) {
        return;
    }
    language_ = language;
    i18n::set_language(language);
    persist();

    auto alive = alive_;
    platform::post_to_ui([this, alive]() {
        if (!alive->load()) {
            return;
        }
        if (ui_refresh_callback_) {
            ui_refresh_callback_();
        }
        set_title();
        set_status(std::string(i18n::tr("Language: ")) + i18n::language_name(language_));
        publish_settings();
    });
}

void BaseViewModel::set_keyboard_layout(platform::KeyboardLayout layout) {
    if (keyboard_layout_ == layout) {
        return;
    }
    keyboard_layout_ = layout;
    platform::set_keyboard_layout(layout);
    persist();
    publish_settings();
    set_status(std::string(i18n::tr("Keyboard layout: ")) +
               (layout == platform::KeyboardLayout::It ? "IT" : "US"));
}

void BaseViewModel::check_dependencies() {
    set_status(i18n::tr("Checking yt-dlp, mpv and curl..."));
    auto alive = alive_;
    platform::submit_background([this, alive]() {
        const auto status = platform::ytdlp::dependencies();
        const std::string summary = status.summary();
        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, status, summary, alive]() {
            if (!alive->load()) {
                return;
            }
            dependencies_checked_ = true;
            publish_settings();
            if (status.ready()) {
                set_status(summary);
            }
            else {
                set_status(summary + i18n::tr(" - install them with apt or pip"));
            }
        });
    });
}

void BaseViewModel::update_ytdlp() {
    set_status(i18n::tr("Updating yt-dlp, this can take a minute..."));
    auto alive = alive_;
    platform::submit_background([this, alive]() {
        const auto result = platform::ytdlp::update();
        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, result, alive]() {
            if (!alive->load()) {
                return;
            }
            set_status(result.message);
            publish_settings();
        });
    });
}

void BaseViewModel::show_version_info() {
    set_status(i18n::tr("Reading versions..."));
    auto alive = alive_;
    platform::submit_background([this, alive]() {
        const std::string ytdlp_version = platform::ytdlp::version();
        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, ytdlp_version, alive]() {
            if (!alive->load()) {
                return;
            }
            std::string message = "CardTube ";
            message += APP_VERSION;
            message += " | yt-dlp ";
            message += ytdlp_version.empty() ? i18n::tr("not installed") : ytdlp_version;
            set_status(message);
        });
    });
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------

bool BaseViewModel::handle_key(uint32_t key, const char* key_name, bool long_pressed, bool repeat) {
    LV_UNUSED(key_name);

    if (long_pressed) {
        return false;
    }

    if (key == LV_KEY_NEXT) {
        cycle_page(1);
        return true;
    }
    if (key == LV_KEY_PREV) {
        cycle_page(-1);
        return true;
    }

    switch (model_.current_page()) {
        case model::AppPage::Search:
            if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
                backspace_query();
                return true;
            }
            if (key == LV_KEY_ENTER) {
                start_search();
                return true;
            }
            if (key == LV_KEY_ESC) {
                return false; // the app layer turns this into the exit gesture
            }
            if (key >= 32 && key != 127) {
                if (repeat) {
                    return true;
                }
                append_query_codepoint(key);
                return true;
            }
            return false;

        case model::AppPage::Results:
            if (key == LV_KEY_UP) {
                move_result_selection(-1);
                return true;
            }
            if (key == LV_KEY_DOWN) {
                move_result_selection(1);
                return true;
            }
            if (key == LV_KEY_ENTER) {
                play_selected();
                return true;
            }
            if (key == 'v' || key == 'V') {
                play_result_video(model_.selected_result());
                return true;
            }
            if (key == 'q' || key == 'Q') {
                enqueue_selected();
                return true;
            }
            if (key == ' ') {
                play_selected();
                return true;
            }
            if (key == LV_KEY_ESC || key == '/') {
                open_search_page();
                return true;
            }
            return false;

        case model::AppPage::NowPlaying:
            if (key == ' ') {
                toggle_pause();
                return true;
            }
            if (key == LV_KEY_LEFT) {
                seek_relative(-kSeekStepSeconds);
                return true;
            }
            if (key == LV_KEY_RIGHT) {
                seek_relative(kSeekStepSeconds);
                return true;
            }
            if (key == '-' || key == '_') {
                change_volume(-1);
                return true;
            }
            if (key == '+' || key == '=') {
                change_volume(1);
                return true;
            }
            if (key == 'n' || key == 'N') {
                play_next();
                return true;
            }
            if (key == 'p' || key == 'P') {
                play_previous();
                return true;
            }
            if (key == 'v' || key == 'V') {
                play_now_as_video();
                return true;
            }
            if (key == 's' || key == 'S') {
                stop_playback();
                return true;
            }
            if (key == LV_KEY_ESC) {
                set_page(model::AppPage::Results);
                return true;
            }
            return false;

        case model::AppPage::Queue:
            if (key == LV_KEY_UP) {
                move_queue_selection(-1);
                return true;
            }
            if (key == LV_KEY_DOWN) {
                move_queue_selection(1);
                return true;
            }
            if (key == LV_KEY_ENTER || key == ' ') {
                play_queue_index(model_.queue_selection());
                return true;
            }
            if (key == LV_KEY_DEL || key == LV_KEY_BACKSPACE || key == 'd' || key == 'D') {
                remove_queue_selection();
                return true;
            }
            if (key == 'c' || key == 'C') {
                clear_queue();
                return true;
            }
            if (key == LV_KEY_ESC) {
                set_page(model::AppPage::NowPlaying);
                return true;
            }
            return false;

        case model::AppPage::Settings:
            if (key == LV_KEY_UP) {
                move_settings_selection(-1);
                return true;
            }
            if (key == LV_KEY_DOWN) {
                move_settings_selection(1);
                return true;
            }
            if (key == LV_KEY_LEFT) {
                adjust_setting(-1);
                return true;
            }
            if (key == LV_KEY_RIGHT) {
                adjust_setting(1);
                return true;
            }
            if (key == LV_KEY_ENTER || key == ' ') {
                activate_setting();
                return true;
            }
            if (key == 'r' || key == 'R') {
                check_dependencies();
                return true;
            }
            if (key == 'u' || key == 'U') {
                update_ytdlp();
                return true;
            }
            if (key == 'i' || key == 'I') {
                show_version_info();
                return true;
            }
            if (key == LV_KEY_ESC) {
                open_search_page();
                return true;
            }
            return false;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void BaseViewModel::load_config(const std::string& path) {
    config_path_ = path;

    config::ApplicationConfig application_config;
    std::string error;
    if (!config::load_application_config(path, application_config, error)) {
        LOG_WARN("could not load config {}: {}", path, error);
        return;
    }

    model_.set_dark_mode(application_config.dark_mode);
    model_.set_video_mode(application_config.video_mode);
    model_.set_autoplay_next(application_config.autoplay_next);
    model_.set_volume(application_config.volume);
    model_.set_quality(config::normalize_quality(application_config.quality));

    language_ = i18n::language_from_code(application_config.language);
    i18n::set_language(language_);
    keyboard_layout_ = config::normalize_keyboard_layout(application_config.keyboard_layout) == "it"
                           ? platform::KeyboardLayout::It
                           : platform::KeyboardLayout::Us;
    platform::set_keyboard_layout(keyboard_layout_);

    publish_all();
    set_status(page_hint(model_.current_page()));
    LOG_INFO("loaded config: {}", path);
}

void BaseViewModel::set_config_path(const std::string& path) {
    config_path_ = path;
}

void BaseViewModel::persist() {
    if (config_path_.empty()) {
        return;
    }

    config::ApplicationConfig application_config;
    application_config.dark_mode = model_.dark_mode();
    application_config.video_mode = model_.video_mode();
    application_config.autoplay_next = model_.autoplay_next();
    application_config.volume = model_.volume();
    application_config.quality = model_.quality();
    application_config.language = i18n::language_code(language_);
    application_config.keyboard_layout =
        keyboard_layout_ == platform::KeyboardLayout::It ? "it" : "us";

    std::string error;
    if (!config::save_application_config(config_path_, application_config, error)) {
        LOG_WARN("could not save config {}: {}", config_path_, error);
    }
    else {
        LOG_INFO("saved config: {}", config_path_);
    }
}

void BaseViewModel::schedule_save() {
    if (!save_timer_) {
        persist();
        return;
    }
    lv_timer_reset(save_timer_);
    lv_timer_resume(save_timer_);
}

void BaseViewModel::save_config_now() {
    if (save_timer_) {
        lv_timer_pause(save_timer_);
    }
    persist();
}

void BaseViewModel::save_timer_cb(lv_timer_t* timer) {
    auto* view_model = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (view_model) {
        view_model->persist();
    }
}

// ---------------------------------------------------------------------------
// Playback monitor
// ---------------------------------------------------------------------------

void BaseViewModel::monitor_timer_cb(lv_timer_t* timer) {
    auto* view_model = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (!view_model || view_model->video_attempt_.load() || view_model->video_ready_.load()) {
        return;
    }

    if (!platform::mpv::running()) {
        if (view_model->playing_subject_.value()) {
            view_model->playing_subject_.set(false);
        }
        return;
    }

    const auto snapshot = platform::mpv::snapshot();

    const int position = static_cast<int>(snapshot.position_s);
    const int duration = static_cast<int>(snapshot.duration_s);
    int progress = 0;
    if (duration > 0) {
        progress = static_cast<int>((snapshot.position_s / duration) * 1000.0);
        progress = std::clamp(progress, 0, 1000);
    }

    view_model->position_subject_.set(position);
    view_model->duration_subject_.set(duration);
    view_model->progress_subject_.set(progress);
    view_model->paused_subject_.set(snapshot.paused);
    view_model->playing_subject_.set(snapshot.playing && !snapshot.paused);

    if (snapshot.ended) {
        platform::mpv::clear_ended();
        view_model->playing_subject_.set(false);

        const auto& queue = view_model->model_.queue();
        const int next = view_model->model_.queue_index() + 1;
        const bool can_advance = view_model->model_.autoplay_next() &&
                                 next < static_cast<int>(queue.size());

        if (can_advance && snapshot.position_s > 3.0) {
            view_model->play_queue_index(next);
        }
        else if (can_advance) {
            view_model->set_status(
                i18n::tr("Playback ended early - press N to try the next track"));
        }
        else {
            view_model->set_status(i18n::tr("Finished"));
        }
    }
}

// ---------------------------------------------------------------------------
// Video session
// ---------------------------------------------------------------------------

bool BaseViewModel::video_starting() {
    return video_attempt_.load() && !video_ready_.load() && !video_failed_.load();
}

bool BaseViewModel::video_running() {
    if (!video_ready_.load()) {
        return false;
    }
    return platform::video::active();
}

void BaseViewModel::finish_video() {
    if (!video_ready_.exchange(false)) {
        return;
    }

    platform::video::stop();
    playing_subject_.set(false);
    paused_subject_.set(true);
    set_page(model::AppPage::NowPlaying);
    set_status(i18n::tr("Video finished - Space resumes audio"));
    lv_obj_invalidate(lv_screen_active());
}

void BaseViewModel::stop_video() {
    if (video_ready_.load()) {
        platform::video::stop();
        set_status(i18n::tr("Video closed"));
        return;
    }
    if (video_attempt_.load()) {
        video_cancel_.store(true);
        set_status(i18n::tr("Cancelling video..."));
    }
}

bool BaseViewModel::take_video_error(std::string& error) {
    if (!video_failed_.exchange(false)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(video_error_mutex_);
        error = video_error_;
    }
    set_page(model::AppPage::NowPlaying);
    set_status(std::string(i18n::tr("Video failed: ")) + error);
    LOG_WARN("video playback failed: {}", error);
    lv_obj_invalidate(lv_screen_active());
    return true;
}

void BaseViewModel::startup_check() {
    auto alive = alive_;
    platform::submit_background([this, alive]() {
        const auto status = platform::ytdlp::dependencies();
        const std::string summary = status.summary();
        if (!alive->load()) {
            return;
        }
        platform::post_to_ui([this, status, summary, alive]() {
            if (!alive->load()) {
                return;
            }
            dependencies_checked_ = true;
            publish_settings();
            if (status.ready()) {
                set_status(i18n::tr("Ready - type a search, Tab switches pages"));
            }
            else {
                set_status(summary + i18n::tr(" - see Settings > Dependencies"));
            }
        });
    });
}

void BaseViewModel::load_demo_state() {
    model_.set_query("lofi hip hop radio");
    model_.results() = {
        {"demo0000001",
         "Midnight Drive - Synthwave Mix 2026",
         "Neon Roads",
         192},
        {"demo0000002",
         "Lofi Beats to Code To - 24/7 Radio",
         "Chill Turtle",
         3660},
        {"demo0000003",
         "Cardputer Zero - Linux in Your Pocket",
         "Maker Notes",
         761},
        {"demo0000004",
         "Retro Game Soundtracks - Live Set",
         "Pixel Arcade",
         1697},
        {"demo0000005",
         "Rainy Window - Ambient Piano",
         "Paper Lantern",
         2702},
    };
    model_.set_selected_result(1);

    model_.queue() = {
        model_.results()[0],
        model_.results()[1],
        model_.results()[2],
        model_.results()[3],
    };
    model_.set_queue_selection(1);
    model_.set_queue_index(0);
    model_.set_now_playing(model_.results()[0]);

    model_.set_dark_mode(true);
    model_.set_video_mode(false);
    model_.set_autoplay_next(true);
    model_.set_volume(70);
    model_.set_quality("best");

    dependencies_checked_ = true;
    publish_all();

    playing_subject_.set(true);
    paused_subject_.set(false);
    position_subject_.set(74);
    duration_subject_.set(213);
    progress_subject_.set(347);

    model_.set_current_page(model::AppPage::Search);
    current_page_subject_.set(page_to_int(model::AppPage::Search));
    set_title();
    set_status(page_hint(model::AppPage::Search));
    prefetch_thumbnails(model_.results());
    LOG_INFO("demo state loaded");
}

} // namespace viewmodel
