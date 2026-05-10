#ifndef __APP_H__
#define __APP_H__

#include <Arduino.h>
#include "../hardware/DisplayManager.h"
#include "../hardware/SpiMutex.h"
#include "../storage/StorageManager.h"
#include "../network/NetworkManager.h"
#include "../playback/PlaybackManager.h"

extern DisplayManager g_display;
extern StorageManager g_storage;
extern CydNetworkManager g_network;
extern PlaybackManager g_playback;

class Application {
private:
    static Application* instance;
    bool initialized;
    bool showing_connection_info;
    bool playback_was_running_before_info;
    bool touch_was_down;
    uint32_t last_touch_toggle_ms;

    void handle_touch();
    void show_connection_info();
    void show_photo_display();

public:
    Application();
    ~Application();

    static Application* get_instance() { return instance; }

    void init();
    void run();

    DisplayManager* get_display() { return &g_display; }
    StorageManager* get_storage() { return &g_storage; }
    CydNetworkManager* get_network() { return &g_network; }
    PlaybackManager* get_playback() { return &g_playback; }
};

#endif // __APP_H__
