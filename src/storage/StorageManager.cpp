#include "../../include/storage/StorageManager.h"
#include "../../include/hardware/SpiMutex.h"
#include <cstring>

namespace {
const char* basename_for_path(const char* path) {
    if (!path) {
        return "";
    }

    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
}

StorageManager::StorageManager() : sd_initialized(false), sd_present(false), total_space(0), free_space(0) {
}

StorageManager::~StorageManager() {
    if (sd_initialized) {
        SD.end();
    }
}

void StorageManager::init(SPIClass* shared_spi) {
    Serial.println("[Storage] Initializing SD card...");

    (void)shared_spi;

    Serial.println("[Storage] Mount mode: PRIDE exact SD.begin(5) test");

    if (!SD.begin(SD_CHIP_SELECT)) {
        Serial.println("[Storage] ERROR: SD.begin(5) failed");
        sd_initialized = false;
        sd_present = false;
        return;
    }

    sd_initialized = true;

    // Check if SD card is actually present
    if (detect_sd_card()) {
        sd_present = true;
        create_image_directory();
        Serial.println("[Storage] SD card ready");
    } else {
        Serial.println("[Storage] WARNING: SD card not detected");
    }
}

bool StorageManager::detect_sd_card() {
    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    if (SD.cardType() == CARD_NONE) {
        return false;
    }

    File root = SD.open("/");
    if (!root) {
        return false;
    }

    root.close();
    total_space = SD.totalBytes();
    free_space = total_space - SD.usedBytes();
    Serial.printf("[Storage] Card type=%u total=%llu free=%llu\n",
                  SD.cardType(),
                  static_cast<unsigned long long>(total_space),
                  static_cast<unsigned long long>(free_space));
    return true;
}

bool StorageManager::create_image_directory() {
    if (!sd_initialized) return false;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    if (!SD.exists(IMAGES_DIR)) {
        if (!SD.mkdir(IMAGES_DIR)) {
            Serial.printf("[Storage] Failed to create directory %s\n", IMAGES_DIR);
            return false;
        }
    }
    return true;
}

uint32_t StorageManager::get_next_image_number() {
    if (!sd_initialized) return 1;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return 1;

    uint32_t max_num = 0;
    File dir = SD.open(IMAGES_DIR);

    if (!dir) return 1;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            // Try to parse number from filename
            const char* name = basename_for_path(file.name());
            if (name) {
                uint32_t num = 0;
                sscanf(name, "img_%04u", &num);
                if (num > max_num) {
                    max_num = num;
                }
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    return max_num + 1;
}

std::string StorageManager::generate_filename(const char* extension) {
    char buf[64];
    uint32_t num = get_next_image_number();
    snprintf(buf, sizeof(buf), IMAGES_DIR "/img_%04u.%s", num, extension);
    return std::string(buf);
}

bool StorageManager::file_exists(const char* path) {
    if (!sd_initialized) return false;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    return SD.exists(path);
}

bool StorageManager::delete_file(const char* path) {
    if (!sd_initialized) return false;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    if (SD.remove(path)) {
        Serial.printf("[Storage] Deleted: %s\n", path);
        return true;
    }

    Serial.printf("[Storage] Failed to delete: %s\n", path);
    return false;
}

bool StorageManager::delete_all_images() {
    if (!sd_initialized) return false;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    File dir = SD.open(IMAGES_DIR);
    if (!dir) return false;

    File file = dir.openNextFile();
    int deleted = 0;

    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            char path[128];
            snprintf(path, sizeof(path), "%s/%s", IMAGES_DIR, basename_for_path(name));

            if (SD.remove(path)) {
                deleted++;
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    Serial.printf("[Storage] Deleted %d images\n", deleted);
    return true;
}

uint64_t StorageManager::get_free_space() {
    if (!sd_initialized) return 0;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return 0;

    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    if (used >= total) {
        return 0;
    }

    free_space = total - used;
    return free_space;
}

bool StorageManager::has_sufficient_space(uint64_t required_bytes) {
    uint64_t free = get_free_space();
    return free > (required_bytes + SD_MIN_FREE_SPACE);
}

File StorageManager::open_upload_file(const char* extension, std::string& out_filename) {
    if (!sd_initialized) return File();

    out_filename = generate_filename(extension);

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return File();

    File f = SD.open(out_filename.c_str(), FILE_WRITE);
    if (f) {
        Serial.printf("[Storage] Opened upload file: %s\n", out_filename.c_str());
    } else {
        Serial.printf("[Storage] Failed to open upload file: %s\n", out_filename.c_str());
    }
    return f;
}

bool StorageManager::finalize_upload(const char* path) {
    // On successful upload, file is already closed
    Serial.printf("[Storage] Finalized upload: %s\n", path);
    return true;
}

bool StorageManager::cancel_upload(const char* path) {
    if (!sd_initialized) return false;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return false;

    if (SD.remove(path)) {
        Serial.printf("[Storage] Cancelled upload, deleted: %s\n", path);
        return true;
    }
    return false;
}

File StorageManager::open_file_read(const char* path) {
    if (!sd_initialized) return File();

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return File();

    return SD.open(path, FILE_READ);
}

std::vector<FileInfo> StorageManager::list_images() {
    std::vector<FileInfo> result;

    if (!sd_initialized) return result;

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) return result;

    File dir = SD.open(IMAGES_DIR);
    if (!dir) return result;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            if (name) {
                const char* base_name = basename_for_path(name);
                std::string lower_name = base_name;
                for (auto &c : lower_name) c = tolower(c);

                bool is_img = (lower_name.find(".jpg") != std::string::npos ||
                              lower_name.find(".jpeg") != std::string::npos ||
                              lower_name.find(".gif") != std::string::npos);

                if (is_img) {
                    FileInfo info;
                    info.name = base_name;
                    info.size = file.size();
                    info.modified = 0; // Would need RTC for actual time
                    info.is_image = true;
                    result.push_back(info);
                }
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    return result;
}
