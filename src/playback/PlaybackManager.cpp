#include "../../include/playback/PlaybackManager.h"
#include "../../include/playback/ImageUtils.h"
#include "../../include/hardware/DisplayManager.h"
#include "../../include/hardware/SpiMutex.h"
#include "../../include/storage/StorageManager.h"

extern DisplayManager g_display;
extern StorageManager g_storage;

PlaybackManager* PlaybackManager::instance = nullptr;

void PlaybackManager::playback_task_fn(void* arg) {
    if (!instance) return;

    while (1) {
        if (instance->playing && !instance->paused) {
            bool rendered_this_cycle = false;

            if (instance->render_requested ||
                instance->last_rendered_index != instance->current_index) {
                const char* current = instance->get_current_image();
                if (current) {
                    std::string lower = current;
                    for (auto &c : lower) c = tolower(c);

                    bool is_gif = (lower.find(".gif") != std::string::npos);
                    bool is_jpg = (lower.find(".jpg") != std::string::npos ||
                                  lower.find(".jpeg") != std::string::npos);

                    char path[256];
                    snprintf(path, sizeof(path), "/images/%s", current);

                    if (is_jpg) {
                        rendered_this_cycle = instance->render_jpg(path);
                    } else if (is_gif) {
                        rendered_this_cycle = instance->render_gif(path);
                    }

                    if (rendered_this_cycle) {
                        instance->last_rendered_index = instance->current_index;
                        instance->render_requested = false;
                    }
                }
            }

            if (instance->image_list.size() <= 1) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // Wait for configured delay
            vTaskDelay(pdMS_TO_TICKS(instance->delay_ms));

            // Move to next image
            if (instance->playing && !instance->paused) {
                instance->next();
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

PlaybackManager::PlaybackManager()
    : current_index(0), mode(PLAYBACK_SEQUENTIAL), delay_ms(3000),
      playing(false), paused(false), render_requested(true),
      last_rendered_index(static_cast<size_t>(-1)), playback_task(nullptr) {
    instance = this;
}

PlaybackManager::~PlaybackManager() {
    instance = nullptr;
    if (playback_task) {
        vTaskDelete(playback_task);
    }
}

void PlaybackManager::init() {
    Serial.println("[Playback] Initializing...");

    refresh_image_list();

    // Create playback task (Core 0)
    xTaskCreatePinnedToCore(
        playback_task_fn,
        "Playback",
        8192,
        nullptr,
        1,
        &playback_task,
        0
    );

    Serial.printf("[Playback] Initialized with %zu images\n", image_list.size());
}

void PlaybackManager::refresh_image_list() {
    image_list.clear();

    auto files = g_storage.list_images();
    for (const auto& file : files) {
        image_list.push_back(file.name);
    }

    current_index = 0;
    last_rendered_index = static_cast<size_t>(-1);
    render_requested = true;
    Serial.printf("[Playback] Found %zu images\n", image_list.size());
}

void PlaybackManager::start() {
    if (!image_list.empty()) {
        Serial.println("[Playback] Clearing screen before image playback");
        g_display.clear_overlay();
        playing = true;
        paused = false;
        render_requested = true;
        Serial.println("[Playback] Started");
    } else {
        Serial.println("[Playback] No images to play");
    }
}

void PlaybackManager::stop() {
    playing = false;
    paused = false;
    g_display.clear_overlay();
    Serial.println("[Playback] Stopped");
}

void PlaybackManager::pause() {
    if (playing && !paused) {
        paused = true;
        Serial.println("[Playback] Paused");
    }
}

void PlaybackManager::resume() {
    paused = false;
    Serial.println("[Playback] Resumed");
}

void PlaybackManager::reload_images() {
    refresh_image_list();
}

void PlaybackManager::next() {
    if (image_list.empty()) return;

    if (mode == PLAYBACK_SEQUENTIAL) {
        current_index = (current_index + 1) % image_list.size();
    } else if (mode == PLAYBACK_RANDOM) {
        current_index = random(image_list.size());
    }
    render_requested = true;
}

void PlaybackManager::previous() {
    if (image_list.empty()) return;

    if (current_index == 0) {
        current_index = image_list.size() - 1;
    } else {
        current_index--;
    }
    render_requested = true;
}

bool PlaybackManager::show_current_image() {
    const char* current = get_current_image();
    if (!current) {
        Serial.println("[Playback] No current image to render");
        return false;
    }

    std::string lower = current;
    for (auto &c : lower) c = tolower(c);

    char path[256];
    snprintf(path, sizeof(path), "/images/%s", current);

    bool rendered = false;
    if (lower.find(".jpg") != std::string::npos ||
        lower.find(".jpeg") != std::string::npos) {
        rendered = render_jpg(path);
    } else if (lower.find(".gif") != std::string::npos) {
        rendered = render_gif(path);
    }

    if (rendered) {
        last_rendered_index = current_index;
        render_requested = false;
    }

    return rendered;
}

void PlaybackManager::set_mode(PlaybackMode m) {
    mode = m;
    Serial.printf("[Playback] Mode: %s\n", (m == PLAYBACK_SEQUENTIAL) ? "Sequential" : "Random");
}

void PlaybackManager::set_delay(uint32_t ms) {
    delay_ms = ms;
    Serial.printf("[Playback] Delay: %u ms\n", ms);
}

const char* PlaybackManager::get_current_image() const {
    if (current_index >= image_list.size()) return nullptr;
    return image_list[current_index].c_str();
}

bool PlaybackManager::render_jpg(const char* path) {
    Serial.printf("[Playback] Rendering JPG: %s\n", path);

    File file = g_storage.open_file_read(path);
    if (!file) {
        Serial.printf("[Playback] ERROR: Could not open file: %s\n", path);
        return false;
    }

    g_display.begin_image_render();
    bool rendered = ImageUtils::render_jpg_optimized(g_display.get_tft(), file, -1, -1);

    {
        SpiMutex::Guard spi_guard;
        if (spi_guard.is_locked()) {
            file.close();
        }
    }

    if (!rendered) {
        Serial.printf("[Playback] ERROR: JPG render failed: %s\n", path);
    }
    return rendered;
}

bool PlaybackManager::render_gif(const char* path) {
    Serial.printf("[Playback] Rendering GIF: %s\n", path);

    File file = g_storage.open_file_read(path);
    if (!file) {
        Serial.printf("[Playback] ERROR: Could not open file: %s\n", path);
        return false;
    }

    // TODO: Implement GIF rendering with frame timing
    g_display.begin_image_render();
    {
        SpiMutex::Guard spi_guard;
        if (!spi_guard.is_locked()) {
            Serial.println("[Playback] ERROR: Could not acquire SPI mutex for rendering");
            file.close();
            return false;
        }
        g_display.get_tft()->fillScreen(TFT_BLACK);
    }
    g_display.show_playback_info("GIF playback pending", current_index + 1, image_list.size());

    {
        SpiMutex::Guard spi_guard;
        if (spi_guard.is_locked()) {
            file.close();
        }
    }
    return true;
}
