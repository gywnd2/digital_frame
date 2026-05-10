#include "../../include/app/Application.h"

// Global instances for access from C callbacks
DisplayManager g_display;
StorageManager g_storage;
CydNetworkManager g_network;
PlaybackManager g_playback;

Application* Application::instance = nullptr;

Application::Application()
    : initialized(false), showing_connection_info(false),
      playback_was_running_before_info(false), touch_was_down(false),
      last_touch_toggle_ms(0) {
    instance = this;
}

Application::~Application() {
    instance = nullptr;
}

void Application::init() {
    if (initialized) return;

    Serial.println("\n\n===== CYD Digital Frame =====");
    Serial.println("Initializing system...\n");

    // Initialize SPI mutex (must be first)
    SpiMutex::init();

    // Initialize storage before TFT, matching the PRIDE CYD hardware init order.
    Serial.println("[App] Initializing storage...");
    g_storage.init();

    // Initialize display (Core 0)
    Serial.println("[App] Initializing display...");
    g_display.init();

    // Check SD card
    if (!g_storage.is_sd_ready()) {
        Serial.println("[App] ERROR: SD card not ready");
        g_display.show_sd_card_error();
        // Continue without SD card - WiFi and web server can still work
    }

    // Initialize playback (only if SD card is ready)
    if (g_storage.is_sd_ready()) {
        Serial.println("[App] Initializing playback...");
        g_playback.init();
    } else {
        Serial.println("[App] Skipping playback initialization (no SD card)");
    }

    // Initialize WiFi (Core 1)
    Serial.println("[App] Initializing network...");
    g_network.init(WIFI_SSID, WIFI_PASS);

    // Start web server
    Serial.println("[App] Starting web server...");
    g_network.start_server();

    if (g_storage.is_sd_ready()) {
        g_playback.start();
    }

    initialized = true;
    Serial.println("\n[App] System ready!\n");
}

void Application::run() {
    // Main loop - runs on Core 1 alongside WiFi
    handle_touch();

    vTaskDelay(pdMS_TO_TICKS(40));
}

void Application::handle_touch() {
    if (!initialized || !g_display.is_initialized()) {
        return;
    }

    const bool touch_down = g_display.is_touch_pressed();
    const uint32_t now = millis();

    if (touch_down && !touch_was_down &&
        now - last_touch_toggle_ms > 350) {
        last_touch_toggle_ms = now;

        if (showing_connection_info) {
            show_photo_display();
        } else {
            show_connection_info();
        }
    }

    touch_was_down = touch_down;
}

void Application::show_connection_info() {
    showing_connection_info = true;
    playback_was_running_before_info = g_playback.is_playing() && !g_playback.is_paused();

    if (playback_was_running_before_info) {
        g_playback.pause();
    }

    if (g_network.get_state() == WIFI_CONNECTED) {
        Serial.println("[App] Touch toggle: showing WiFi connection info");
        g_display.show_wifi_connected(g_network.get_ssid(), g_network.get_ip());
    } else {
        Serial.println("[App] Touch toggle: showing WiFi connecting info");
        g_display.show_wifi_connecting(g_network.get_ssid());
    }
}

void Application::show_photo_display() {
    showing_connection_info = false;
    Serial.println("[App] Touch toggle: showing photo display");

    if (g_storage.is_sd_ready() && g_playback.get_total_images() > 0) {
        g_playback.show_current_image();
    } else {
        g_display.clear_overlay();
    }

    if (playback_was_running_before_info) {
        g_playback.resume();
    }

    playback_was_running_before_info = false;
}
