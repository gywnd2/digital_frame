#ifndef __DISPLAY_MANAGER_H__
#define __DISPLAY_MANAGER_H__

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <string>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define LVGL_BUFFER_PIXELS 4096

class DisplayManager {
private:
    TFT_eSPI tft;
    lv_display_t* display;
    lv_color_t buf[LVGL_BUFFER_PIXELS];
    TaskHandle_t lvgl_task;
    bool sd_error_active;
    volatile bool direct_render_active;
    lv_obj_t* upload_filename_label;
    lv_obj_t* upload_percent_label;
    lv_obj_t* upload_bar;
    std::string upload_filename;
    int upload_percent;

    static DisplayManager* instance;
    static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    static uint32_t lvgl_tick_cb();
    static void lvgl_task_fn(void* arg);
    void fill_tft_black();
    void reset_upload_ui();

public:
    DisplayManager();
    ~DisplayManager();

    void init();
    bool is_initialized() const { return display != nullptr; }
    void show_sd_card_error();
    void show_wifi_connecting(const char* ssid);
    void show_wifi_connected(const char* ssid, const char* ip);
    void show_upload_progress(int percent, const char* filename);
    void show_upload_complete();
    void show_playback_info(const char* filename, int current, int total);
    void clear_overlay();
    void begin_image_render();
    bool is_touch_pressed(uint16_t threshold = 600);

    // Direct rendering methods for images
    void draw_jpg_from_file(const char* path, int16_t x, int16_t y);
    void draw_jpg_stream(uint8_t* data, size_t len, int16_t x, int16_t y);

    TFT_eSPI* get_tft() { return &tft; }
    SPIClass& get_spi() { return TFT_eSPI::getSPIinstance(); }
};

#endif // __DISPLAY_MANAGER_H__
