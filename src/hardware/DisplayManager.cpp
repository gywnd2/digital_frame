#include "../../include/hardware/DisplayManager.h"
#include "../../include/hardware/SpiMutex.h"

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_32);

DisplayManager* DisplayManager::instance = nullptr;

namespace {
SemaphoreHandle_t lvgl_mutex = nullptr;

bool lock_lvgl(TickType_t timeout = portMAX_DELAY) {
    return !lvgl_mutex || xSemaphoreTake(lvgl_mutex, timeout) == pdTRUE;
}

void unlock_lvgl() {
    if (lvgl_mutex) {
        xSemaphoreGive(lvgl_mutex);
    }
}

class LvglGuard {
public:
    explicit LvglGuard(TickType_t timeout = pdMS_TO_TICKS(1000)) : locked(lock_lvgl(timeout)) {}
    ~LvglGuard() {
        if (locked) {
            unlock_lvgl();
        }
    }
    bool is_locked() const { return locked; }

private:
    bool locked;
};

void set_screen_black(bool clean = true) {
    lv_obj_t* screen = lv_scr_act();
    if (clean) {
        lv_obj_clean(screen);
    }
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

void set_label_white(lv_obj_t* label, const lv_font_t* font = &lv_font_montserrat_28) {
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, font, 0);
}

void fit_center_label(lv_obj_t* label, int y, const lv_font_t* font = &lv_font_montserrat_28) {
    lv_obj_set_width(label, SCREEN_WIDTH - 20);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    set_label_white(label, font);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
}
}

void DisplayManager::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (!instance) {
        lv_display_flush_ready(disp);
        return;
    }

    if (instance->direct_render_active) {
        lv_display_flush_ready(disp);
        return;
    }

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) {
        lv_display_flush_ready(disp);
        return;
    }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    if (LV_COLOR_16_SWAP) {
        size_t len = lv_area_get_size(area);
        lv_draw_sw_rgb565_swap(px_map, len);
    }

    instance->tft.startWrite();
    instance->tft.setAddrWindow(area->x1, area->y1, w, h);
    instance->tft.pushColors((uint16_t*)px_map, w * h, true);
    instance->tft.endWrite();

    lv_display_flush_ready(disp);
}

uint32_t DisplayManager::lvgl_tick_cb() {
    return millis();
}

void DisplayManager::lvgl_task_fn(void* arg) {
    if (!instance) return;

    while (1) {
        if (lock_lvgl(pdMS_TO_TICKS(100))) {
            lv_timer_handler();
            unlock_lvgl();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

DisplayManager::DisplayManager()
    : tft(), display(nullptr), lvgl_task(nullptr),
      sd_error_active(false), direct_render_active(false),
      upload_filename_label(nullptr), upload_percent_label(nullptr), upload_bar(nullptr),
      upload_filename(), upload_percent(-1) {
    instance = this;
}

DisplayManager::~DisplayManager() {
    instance = nullptr;
}

void DisplayManager::fill_tft_black() {
    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) {
        return;
    }

    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::reset_upload_ui() {
    upload_filename_label = nullptr;
    upload_percent_label = nullptr;
    upload_bar = nullptr;
    upload_filename.clear();
    upload_percent = -1;
}

void DisplayManager::init() {
    Serial.println("[Display] Initializing TFT...");

    // Initialize TFT
    {
        SpiMutex::Guard spi_guard;
        if (!spi_guard.is_locked()) {
            Serial.println("[Display] ERROR: Could not acquire SPI mutex for TFT init");
            return;
        }

        tft.begin();
        tft.setRotation(3); // Landscape: 320x240, counterclockwise from portrait
        tft.fillScreen(TFT_BLACK);
        Serial.printf("[Display] TFT size after rotation: %dx%d\n", tft.width(), tft.height());
        if (tft.width() != SCREEN_WIDTH || tft.height() != SCREEN_HEIGHT) {
            Serial.printf("[Display] WARNING: LVGL %dx%d differs from TFT %dx%d\n",
                          SCREEN_WIDTH, SCREEN_HEIGHT, tft.width(), tft.height());
        }

        // Set backlight
        pinMode(21, OUTPUT);
        digitalWrite(21, HIGH);
    }

    Serial.println("[Display] Initializing LVGL...");

    // LVGL init
    if (!lvgl_mutex) {
        lvgl_mutex = xSemaphoreCreateMutex();
        if (!lvgl_mutex) {
            Serial.println("[Display] ERROR: Failed to create LVGL mutex");
            return;
        }
    }

    lv_init();

    // Create display
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_tick_set_cb(lvgl_tick_cb);

    display = disp;

    // Create theme
    lv_theme_t * th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                            lv_palette_main(LV_PALETTE_RED), false,
                                            &lv_font_montserrat_14);
    lv_disp_set_theme(disp, th);
    set_screen_black(false);
    fill_tft_black();

    // Create LVGL update task (Core 0)
    xTaskCreatePinnedToCore(
        lvgl_task_fn,
        "LVGL",
        4096,
        nullptr,
        2,
        &lvgl_task,
        0
    );

    Serial.println("[Display] Initialized successfully");
}

void DisplayManager::show_sd_card_error() {
    if (!display) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    sd_error_active = true;
    reset_upload_ui();
    fill_tft_black();
    set_screen_black();

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Please Insert\nSD Card!");
    set_label_white(label, &lv_font_montserrat_32);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void DisplayManager::show_wifi_connecting(const char* ssid) {
    if (!display) return;
    if (sd_error_active) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    reset_upload_ui();
    fill_tft_black();
    set_screen_black();

    lv_obj_t *label1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label1, "Connecting to");
    set_label_white(label1, &lv_font_montserrat_24);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, -42);

    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_label_set_text(label2, ssid);
    fit_center_label(label2, 8, &lv_font_montserrat_32);
}

void DisplayManager::show_wifi_connected(const char* ssid, const char* ip) {
    if (!display) return;
    if (sd_error_active) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    reset_upload_ui();
    fill_tft_black();
    set_screen_black();

    lv_obj_t *label1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label1, "Connected to");
    set_label_white(label1, &lv_font_montserrat_24);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *label2 = lv_label_create(lv_scr_act());
    lv_label_set_text(label2, ssid);
    fit_center_label(label2, -12, &lv_font_montserrat_32);

    lv_obj_t *label3 = lv_label_create(lv_scr_act());
    lv_label_set_text(label3, ip);
    fit_center_label(label3, 42, &lv_font_montserrat_28);
}

void DisplayManager::show_upload_progress(int percent, const char* filename) {
    if (!display) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    percent = constrain(percent, 0, 100);
    const char* safe_filename = filename ? filename : "";
    const bool needs_rebuild = !upload_bar || upload_filename != safe_filename;

    if (needs_rebuild) {
        reset_upload_ui();
        upload_filename = safe_filename;

        fill_tft_black();
        set_screen_black();

        upload_filename_label = lv_label_create(lv_scr_act());
        lv_label_set_text(upload_filename_label, safe_filename);
        fit_center_label(upload_filename_label, -54, &lv_font_montserrat_24);

        upload_percent_label = lv_label_create(lv_scr_act());
        set_label_white(upload_percent_label, &lv_font_montserrat_32);
        lv_obj_align(upload_percent_label, LV_ALIGN_CENTER, 0, -6);

        upload_bar = lv_bar_create(lv_scr_act());
        lv_obj_set_size(upload_bar, SCREEN_WIDTH - 40, 28);
        lv_bar_set_range(upload_bar, 0, 100);
        lv_obj_align(upload_bar, LV_ALIGN_CENTER, 0, 62);
    }

    if (upload_percent != percent) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(upload_percent_label, buf);
        lv_bar_set_value(upload_bar, percent, LV_ANIM_OFF);
        upload_percent = percent;
    }
}

void DisplayManager::show_upload_complete() {
    if (!display) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    reset_upload_ui();
    fill_tft_black();
    set_screen_black();

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Upload Complete!");
    fit_center_label(label, 0, &lv_font_montserrat_32);
}

void DisplayManager::show_playback_info(const char* filename, int current, int total) {
    if (!display) return;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    lv_obj_t *label = lv_label_create(lv_scr_act());
    char buf[64];
    snprintf(buf, sizeof(buf), "%s (%d/%d)", filename, current, total);
    lv_label_set_text(label, buf);
    set_label_white(label, &lv_font_montserrat_24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, SCREEN_WIDTH - 10);
    lv_obj_set_style_bg_color(label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_70, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 5, -5);
}

void DisplayManager::clear_overlay() {
    if (!display) return;
    direct_render_active = false;
    LvglGuard lvgl_guard;
    if (!lvgl_guard.is_locked()) return;

    sd_error_active = false;
    reset_upload_ui();
    fill_tft_black();
    set_screen_black();
}

void DisplayManager::begin_image_render() {
    Serial.println("[Display] Preparing image render: clearing LVGL objects and TFT");
    direct_render_active = true;

    if (display) {
        LvglGuard lvgl_guard;
        if (lvgl_guard.is_locked()) {
            lv_obj_clean(lv_scr_act());
            reset_upload_ui();
        }
    }

    fill_tft_black();
}

bool DisplayManager::is_touch_pressed(uint16_t threshold) {
#if defined(TOUCH_CS)
    if (!display) return false;

    if (!SpiMutex::lock(pdMS_TO_TICKS(10))) {
        return false;
    }

    const uint16_t z = tft.getTouchRawZ();
    SpiMutex::unlock();
    return z > threshold;
#else
    (void)threshold;
    return false;
#endif
}

void DisplayManager::draw_jpg_from_file(const char* path, int16_t x, int16_t y) {
    // This will be implemented with optimized line-by-line rendering
    Serial.printf("[Display] TODO: draw_jpg_from_file(%s)\n", path);
}

void DisplayManager::draw_jpg_stream(uint8_t* data, size_t len, int16_t x, int16_t y) {
    // This will be implemented with streaming decoder
    Serial.printf("[Display] TODO: draw_jpg_stream(%zu bytes)\n", len);
}
