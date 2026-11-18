/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

#include <algorithm>

namespace model {
namespace {

int clamp_index(int index, int size) {
    if (size <= 0) {
        return 0;
    }
    return std::clamp(index, 0, size - 1);
}

} // namespace

const char* BaseModel::app_title() const {
    return "CardTube";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}

void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

const std::string& BaseModel::query() const {
    return query_;
}

void BaseModel::set_query(std::string query) {
    query_ = std::move(query);
}

std::vector<TrackInfo>& BaseModel::results() {
    return results_;
}

const std::vector<TrackInfo>& BaseModel::results() const {
    return results_;
}

int BaseModel::selected_result() const {
    return selected_result_;
}

void BaseModel::set_selected_result(int index) {
    selected_result_ = clamp_index(index, static_cast<int>(results_.size()));
}

const TrackInfo* BaseModel::selected_track() const {
    if (results_.empty()) {
        return nullptr;
    }
    return &results_[static_cast<std::size_t>(selected_result_)];
}

std::vector<TrackInfo>& BaseModel::queue() {
    return queue_;
}

const std::vector<TrackInfo>& BaseModel::queue() const {
    return queue_;
}

int BaseModel::queue_selection() const {
    return queue_selection_;
}

void BaseModel::set_queue_selection(int index) {
    queue_selection_ = clamp_index(index, static_cast<int>(queue_.size()));
}

int BaseModel::queue_index() const {
    return queue_index_;
}

void BaseModel::set_queue_index(int index) {
    queue_index_ = queue_.empty() ? -1 : clamp_index(index, static_cast<int>(queue_.size()));
}

const TrackInfo& BaseModel::now_playing() const {
    return now_playing_;
}

void BaseModel::set_now_playing(TrackInfo track) {
    now_playing_ = std::move(track);
}

bool BaseModel::has_now_playing() const {
    return now_playing_.valid();
}

bool BaseModel::video_mode() const {
    return video_mode_;
}

void BaseModel::set_video_mode(bool enabled) {
    video_mode_ = enabled;
}

bool BaseModel::autoplay_next() const {
    return autoplay_next_;
}

void BaseModel::set_autoplay_next(bool enabled) {
    autoplay_next_ = enabled;
}

int BaseModel::volume() const {
    return volume_;
}

void BaseModel::set_volume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
}

const std::string& BaseModel::quality() const {
    return quality_;
}

void BaseModel::set_quality(std::string quality) {
    quality_ = std::move(quality);
}

} // namespace model
