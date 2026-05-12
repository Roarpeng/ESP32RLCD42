#include "photo_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>
#include <algorithm>

// Forward declaration of SdcardManager from the board's managers directory
static const char* TAG = "PhotoManager";

// The listFiles method from SdcardManager is used via extern access.
// For compilation, we forward-declare what we need:
namespace {
    std::vector<std::string> ListPhotoFiles(const char* dir_path) {
        std::vector<std::string> result;
        // Use POSIX dirent to scan directory
        DIR* dir = opendir(dir_path);
        if (!dir) return result;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_REG) continue;
            const char* name = entry->d_name;
            const char* ext = strrchr(name, '.');
            if (!ext) continue;
            if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 ||
                strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".bmp") == 0) {
                std::string full = std::string(dir_path) + "/" + name;
                result.push_back(full);
            }
        }
        closedir(dir);
        return result;
    }
}

PhotoManager& PhotoManager::GetInstance() {
    static PhotoManager instance;
    return instance;
}

PhotoManager::~PhotoManager() {
    StopSlideshow();
}

bool PhotoManager::Initialize(const char* photos_dir) {
    photos_dir_ = photos_dir;
    config_path_ = photos_dir_ + "/photos.json";

    struct stat st;
    if (stat(photos_dir_.c_str(), &st) != 0) {
        mkdir(photos_dir_.c_str(), 0755);
        ESP_LOGI(TAG, "Created photos directory: %s", photos_dir_.c_str());
    }

    config_.Load(config_path_.c_str());
    RefreshList();
    ESP_LOGI(TAG, "Initialized with %d photos in %s", (int)photos_.size(), photos_dir_.c_str());
    return true;
}

void PhotoManager::RefreshList() {
    std::lock_guard<std::mutex> lock(mutex_);
    photos_ = ListPhotoFiles(photos_dir_.c_str());

    std::sort(photos_.begin(), photos_.end());

    if (current_index_ >= (int)photos_.size()) {
        current_index_ = photos_.empty() ? -1 : 0;
    }
    if (current_index_ < 0 && !photos_.empty()) {
        current_index_ = 0;
    }
}

std::string PhotoManager::GetCurrentPhotoPath() const {
    if (current_index_ < 0 || current_index_ >= (int)photos_.size())
        return "";
    return photos_[current_index_];
}

bool PhotoManager::AddPhoto(const char* filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& p : photos_) {
        if (p == filename) return false;
    }
    photos_.push_back(filename);
    std::sort(photos_.begin(), photos_.end());
    if (current_index_ < 0) current_index_ = 0;
    ESP_LOGI(TAG, "Added photo: %s (total: %d)", filename, (int)photos_.size());
    return true;
}

bool PhotoManager::RemovePhoto(const char* filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(photos_.begin(), photos_.end(), filename);
    if (it == photos_.end()) return false;
    int idx = (int)(it - photos_.begin());

    unlink(filename);

    photos_.erase(it);
    if (photos_.empty()) {
        current_index_ = -1;
    } else if (idx <= current_index_) {
        current_index_ = (current_index_ - 1 + photos_.size()) % photos_.size();
    }
    ESP_LOGI(TAG, "Removed photo: %s", filename);
    return true;
}

int PhotoManager::RemovePhotos(const std::vector<std::string>& names) {
    int count = 0;
    for (auto& n : names) {
        if (RemovePhoto(n.c_str())) count++;
    }
    return count;
}

void PhotoManager::StartSlideshow() {
    if (photos_.empty()) {
        ESP_LOGW(TAG, "No photos to start slideshow");
        return;
    }
    state_ = SlideShowState::PLAYING;
    if (current_index_ < 0) current_index_ = 0;

    esp_timer_create_args_t args = {};
    args.callback = &TimerCallback;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "photo_timer";
    if (timer_handle_) {
        esp_timer_stop((esp_timer_handle_t)timer_handle_);
        esp_timer_delete((esp_timer_handle_t)timer_handle_);
    }
    esp_timer_create(&args, (esp_timer_handle_t*)&timer_handle_);
    esp_timer_start_periodic((esp_timer_handle_t)timer_handle_,
                              config_.interval * 1000000ULL);
    ESP_LOGI(TAG, "Slideshow started, interval=%ds", config_.interval);
}

void PhotoManager::StopSlideshow() {
    state_ = SlideShowState::IDLE;
    if (timer_handle_) {
        esp_timer_stop((esp_timer_handle_t)timer_handle_);
        esp_timer_delete((esp_timer_handle_t)timer_handle_);
        timer_handle_ = nullptr;
    }
}

void PhotoManager::TimerCallback(void* arg) {
    PhotoManager* self = (PhotoManager*)arg;
    if (self->state_ != SlideShowState::PLAYING) return;
    self->NextPhoto();
}

void PhotoManager::NextPhoto() {
    if (photos_.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.mode == "random") {
        current_index_ = rand() % photos_.size();
    } else {
        current_index_ = (current_index_ + 1) % photos_.size();
    }
}

void PhotoManager::PrevPhoto() {
    if (photos_.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    current_index_ = (current_index_ - 1 + photos_.size()) % photos_.size();
}

void PhotoManager::Pause() {
    state_ = SlideShowState::PAUSED;
}

void PhotoManager::Resume() {
    if (!photos_.empty()) state_ = SlideShowState::PLAYING;
}

bool PhotoManager::SaveConfig() {
    return config_.Save(config_path_.c_str());
}

void PhotoManager::InvokeTransition(const uint8_t* old_buf, const uint8_t* new_buf, int step) {
    if (on_transition_) {
        on_transition_(old_buf, new_buf, step);
    }
}
