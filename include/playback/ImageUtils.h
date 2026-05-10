#ifndef __IMAGE_UTILS_H__
#define __IMAGE_UTILS_H__

#include <Arduino.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <string>

class ImageUtils {
public:
    // Image format detection
    static bool is_jpg(const char* filename);
    static bool is_gif(const char* filename);

    // Image validation
    static bool validate_jpg_header(const uint8_t* data, size_t len);
    static bool validate_gif_header(const uint8_t* data, size_t len);

    // JPG rendering
    static bool render_jpg_optimized(TFT_eSPI* tft, File& file, int x, int y);

    // GIF rendering
    static bool render_gif_frame(TFT_eSPI* tft, File& file, int x, int y);

    // Image scaling
    static void scale_dimensions(uint32_t src_w, uint32_t src_h,
                                uint32_t max_w, uint32_t max_h,
                                uint32_t& out_w, uint32_t& out_h);

    // File size checking
    static bool check_resolution(const char* filename, bool is_jpg);
};

#endif // __IMAGE_UTILS_H__
