#ifndef __PLAYBACK_MANAGER_H__
#define __PLAYBACK_MANAGER_H__

#include <Arduino.h>
#include <vector>
#include <string>

enum PlaybackMode {
    PLAYBACK_SEQUENTIAL,
    PLAYBACK_RANDOM
};

class PlaybackManager {
private:
    std::vector<std::string> image_list;
    size_t current_index;
    PlaybackMode mode;
    uint32_t delay_ms;
    bool playing;
    bool paused;
    bool render_requested;
    size_t last_rendered_index;
    TaskHandle_t playback_task;

    static PlaybackManager* instance;
    static void playback_task_fn(void* arg);

    bool render_jpg(const char* path);
    bool render_gif(const char* path);
    void refresh_image_list();

public:
    PlaybackManager();
    ~PlaybackManager();

    void init();
    void start();
    void stop();
    void pause();
    void resume();
    void reload_images();
    void next();
    void previous();
    bool show_current_image();

    void set_mode(PlaybackMode m);
    void set_delay(uint32_t ms);

    PlaybackMode get_mode() const { return mode; }
    uint32_t get_delay() const { return delay_ms; }
    bool is_playing() const { return playing; }
    bool is_paused() const { return paused; }
    size_t get_total_images() const { return image_list.size(); }
    size_t get_current_index() const { return current_index; }
    const char* get_current_image() const;
};

#endif // __PLAYBACK_MANAGER_H__
