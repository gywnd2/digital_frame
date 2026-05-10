#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <string>
#include <vector>

#define SD_CHIP_SELECT 5  // GPIO5 for SD card CS, matching the PRIDE CYD project
#define IMAGES_DIR "/images"
#define SD_MIN_FREE_SPACE (1024ULL * 1024ULL) // 1MB minimum

struct FileInfo {
    std::string name;
    uint32_t size;
    time_t modified;
    bool is_image; // JPG or GIF
};

class StorageManager {
private:
    bool sd_initialized;
    bool sd_present;
    uint64_t total_space;
    uint64_t free_space;

    uint32_t get_next_image_number();
    std::string generate_filename(const char* extension);

public:
    StorageManager();
    ~StorageManager();

    void init(SPIClass* shared_spi = nullptr);
    bool is_sd_ready() const { return sd_initialized && sd_present; }

    // Directory operations
    bool create_image_directory();
    std::vector<FileInfo> list_images();

    // File operations
    bool file_exists(const char* path);
    bool delete_file(const char* path);
    bool delete_all_images();
    uint64_t get_free_space();
    bool has_sufficient_space(uint64_t required_bytes);

    // Upload support
    File open_upload_file(const char* extension, std::string& out_filename);
    bool finalize_upload(const char* path);
    bool cancel_upload(const char* path);

    // File reading
    File open_file_read(const char* path);

    // SD card detection
    bool detect_sd_card();
};

#endif // __STORAGE_MANAGER_H__
