/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace model {

enum class AppPage {
    Search     = 0,
    Results    = 1,
    NowPlaying = 2,
    Queue      = 3,
    Settings   = 4,
};

struct TrackInfo {
    std::string id;
    std::string title;
    std::string channel;
    int duration_s = 0;

    bool valid() const {
        return !id.empty();
    }

    std::string url() const {
        return "https://www.youtube.com/watch?v=" + id;
    }
};

class BaseModel {
public:
    const char* app_title() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);

    const std::string& query() const;
    void set_query(std::string query);

    std::vector<TrackInfo>& results();
    const std::vector<TrackInfo>& results() const;
    int selected_result() const;
    void set_selected_result(int index);
    const TrackInfo* selected_track() const;

    std::vector<TrackInfo>& queue();
    const std::vector<TrackInfo>& queue() const;
    int queue_selection() const;
    void set_queue_selection(int index);
    int queue_index() const;
    void set_queue_index(int index);

    const TrackInfo& now_playing() const;
    void set_now_playing(TrackInfo track);
    bool has_now_playing() const;

    bool video_mode() const;
    void set_video_mode(bool enabled);

    bool autoplay_next() const;
    void set_autoplay_next(bool enabled);

    int volume() const;
    void set_volume(int volume);

    const std::string& quality() const;
    void set_quality(std::string quality);

private:
    bool dark_mode_ = true;
    AppPage current_page_ = AppPage::Search;
    std::string query_;
    std::vector<TrackInfo> results_;
    int selected_result_ = 0;
    std::vector<TrackInfo> queue_;
    int queue_selection_ = 0;
    int queue_index_ = -1;
    TrackInfo now_playing_{};
    bool video_mode_ = false;
    bool autoplay_next_ = true;
    int volume_ = 80;
    std::string quality_ = "best";
};

} // namespace model
