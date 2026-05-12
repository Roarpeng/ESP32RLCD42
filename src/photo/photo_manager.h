#ifndef PHOTO_MANAGER_H
#define PHOTO_MANAGER_H

#include "photo_config.h"
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>

enum class SlideShowState { IDLE, PLAYING, PAUSED };

class PhotoManager {
public:
    static PhotoManager& GetInstance();

    bool Initialize(const char* photos_dir);
    void StartSlideshow();
    void StopSlideshow();

    const std::vector<std::string>& GetPhotos() const { return photos_; }
    int GetPhotoCount() const { return photos_.size(); }
    int GetCurrentIndex() const { return current_index_; }
    std::string GetCurrentPhotoPath() const;
    bool AddPhoto(const char* filename);
    bool RemovePhoto(const char* filename);
    int RemovePhotos(const std::vector<std::string>& names);

    void RefreshList();
    void NextPhoto();
    void PrevPhoto();
    void Pause();
    void Resume();
    SlideShowState GetState() const { return state_; }

    PhotoConfig& GetConfig() { return config_; }
    bool SaveConfig();

    using TransitionCallback = std::function<void(const uint8_t* old_buf, const uint8_t* new_buf, int step)>;
    void SetTransitionCallback(TransitionCallback cb) { on_transition_ = cb; }
    void InvokeTransition(const uint8_t* old_buf, const uint8_t* new_buf, int step);

private:
    PhotoManager() = default;
    ~PhotoManager();
    PhotoManager(const PhotoManager&) = delete;
    PhotoManager& operator=(const PhotoManager&) = delete;

    static void TimerCallback(void* arg);

    std::vector<std::string> photos_;
    int current_index_ = -1;
    std::atomic<SlideShowState> state_{SlideShowState::IDLE};
    PhotoConfig config_;
    std::string photos_dir_;
    std::string config_path_;
    TransitionCallback on_transition_;
    void* timer_handle_ = nullptr;
    std::mutex mutex_;
};

#endif
