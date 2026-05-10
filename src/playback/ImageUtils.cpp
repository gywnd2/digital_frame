#include "../../include/playback/ImageUtils.h"
#include "../../include/hardware/SpiMutex.h"
#include <JPEGDecoder.h>

namespace {
uint16_t edge_block_size(uint16_t block, uint16_t image_size) {
    uint16_t remainder = image_size % block;
    return remainder == 0 ? block : remainder;
}

void compact_mcu(uint16_t* pixels,
                 uint16_t src_stride,
                 uint16_t skip_x,
                 uint16_t skip_y,
                 uint16_t draw_w,
                 uint16_t draw_h) {
    if (skip_x == 0 && skip_y == 0 && draw_w == src_stride) {
        return;
    }

    for (uint16_t row = 0; row < draw_h; row++) {
        uint16_t* src = pixels + (skip_y + row) * src_stride + skip_x;
        uint16_t* dst = pixels + row * draw_w;
        for (uint16_t col = 0; col < draw_w; col++) {
            dst[col] = src[col];
        }
    }
}

bool clip_to_screen(uint16_t screen_w,
                    uint16_t screen_h,
                    int& draw_x,
                    int& draw_y,
                    uint16_t& draw_w,
                    uint16_t& draw_h,
                    uint16_t& skip_x,
                    uint16_t& skip_y) {
    skip_x = 0;
    skip_y = 0;

    if (draw_x >= static_cast<int>(screen_w) || draw_y >= static_cast<int>(screen_h) ||
        draw_x + draw_w <= 0 || draw_y + draw_h <= 0) {
        return false;
    }

    if (draw_x < 0) {
        skip_x = static_cast<uint16_t>(-draw_x);
        draw_w -= skip_x;
        draw_x = 0;
    }
    if (draw_y < 0) {
        skip_y = static_cast<uint16_t>(-draw_y);
        draw_h -= skip_y;
        draw_y = 0;
    }
    if (draw_x + draw_w > screen_w) {
        draw_w = screen_w - draw_x;
    }
    if (draw_y + draw_h > screen_h) {
        draw_h = screen_h - draw_y;
    }

    return draw_w > 0 && draw_h > 0;
}

void rotate_ccw_block(const uint16_t* src,
                      uint16_t src_stride,
                      uint16_t src_w,
                      uint16_t src_h,
                      uint16_t* dst) {
    for (uint16_t row = 0; row < src_h; row++) {
        for (uint16_t col = 0; col < src_w; col++) {
            const uint16_t dst_x = row;
            const uint16_t dst_y = src_w - 1 - col;
            dst[dst_y * src_h + dst_x] = src[row * src_stride + col];
        }
    }
}

constexpr uint16_t MAX_LINE_PIXELS = 320;

struct FitRect {
    int x;
    int y;
    uint16_t w;
    uint16_t h;
};

FitRect fit_contain(uint16_t src_w,
                    uint16_t src_h,
                    uint16_t screen_w,
                    uint16_t screen_h,
                    int x,
                    int y) {
    uint32_t target_w = screen_w;
    uint32_t target_h = screen_h;

    if (static_cast<uint32_t>(src_w) * screen_h >
        static_cast<uint32_t>(screen_w) * src_h) {
        target_w = screen_w;
        target_h = (static_cast<uint32_t>(screen_w) * src_h + src_w / 2) / src_w;
    } else {
        target_h = screen_h;
        target_w = (static_cast<uint32_t>(screen_h) * src_w + src_h / 2) / src_h;
    }

    target_w = constrain(target_w, 1U, static_cast<uint32_t>(screen_w));
    target_h = constrain(target_h, 1U, static_cast<uint32_t>(screen_h));

    FitRect rect;
    rect.w = static_cast<uint16_t>(target_w);
    rect.h = static_cast<uint16_t>(target_h);
    rect.x = (x < 0) ? (static_cast<int>(screen_w) - static_cast<int>(rect.w)) / 2 : x;
    rect.y = (y < 0) ? (static_cast<int>(screen_h) - static_cast<int>(rect.h)) / 2 : y;
    return rect;
}

uint16_t scaled_start(uint32_t src_pos, uint16_t dst_size, uint16_t src_size) {
    return static_cast<uint16_t>((src_pos * dst_size) / src_size);
}

uint16_t scaled_end(uint32_t src_pos, uint16_t dst_size, uint16_t src_size) {
    return static_cast<uint16_t>(((src_pos + 1) * dst_size) / src_size);
}

bool append_scaled_run(uint16_t* line,
                       uint16_t& count,
                       bool& has_segment,
                       uint16_t& segment_x,
                       uint16_t x0,
                       uint16_t x1,
                       uint16_t color) {
    if (x1 <= x0) {
        return true;
    }

    if (!has_segment) {
        has_segment = true;
        segment_x = x0;
    }

    while (segment_x + count < x0) {
        if (count >= MAX_LINE_PIXELS) {
            return false;
        }
        line[count++] = color;
    }

    for (uint16_t x = x0; x < x1; x++) {
        if (count >= MAX_LINE_PIXELS) {
            return false;
        }
        line[count++] = color;
    }

    return true;
}

void push_line_segment(TFT_eSPI* tft,
                       uint16_t screen_w,
                       uint16_t screen_h,
                       int x,
                       int y,
                       uint16_t w,
                       const uint16_t* line) {
    if (w == 0 || y < 0 || y >= static_cast<int>(screen_h) ||
        x >= static_cast<int>(screen_w) || x + w <= 0) {
        return;
    }

    uint16_t offset = 0;
    if (x < 0) {
        offset = static_cast<uint16_t>(-x);
        if (offset >= w) {
            return;
        }
        w -= offset;
        x = 0;
    }

    if (x + w > screen_w) {
        w = screen_w - x;
    }

    if (w == 0) {
        return;
    }

    tft->startWrite();
    tft->setAddrWindow(x, y, w, 1);
    tft->pushColors(const_cast<uint16_t*>(line + offset), w, true);
    tft->endWrite();
}

bool draw_scaled_mcu(TFT_eSPI* tft,
                     uint16_t screen_w,
                     uint16_t screen_h,
                     const FitRect& rect,
                     const uint16_t* src,
                     uint16_t src_stride,
                     uint16_t block_w,
                     uint16_t block_h,
                     uint16_t src_x,
                     uint16_t src_y,
                     uint16_t logical_w,
                     uint16_t logical_h) {
    uint16_t line[MAX_LINE_PIXELS];

    for (uint16_t row = 0; row < block_h; row++) {
        const uint16_t logical_y = src_y + row;
        const uint16_t y0 = scaled_start(logical_y, rect.h, logical_h);
        const uint16_t y1 = scaled_end(logical_y, rect.h, logical_h);
        if (y1 <= y0) {
            continue;
        }

        bool has_segment = false;
        uint16_t segment_x = 0;
        uint16_t count = 0;

        for (uint16_t col = 0; col < block_w; col++) {
            const uint16_t logical_x = src_x + col;
            const uint16_t x0 = scaled_start(logical_x, rect.w, logical_w);
            const uint16_t x1 = scaled_end(logical_x, rect.w, logical_w);
            if (!append_scaled_run(line, count, has_segment, segment_x, x0, x1,
                                   src[row * src_stride + col])) {
                return false;
            }
        }

        if (!has_segment || count == 0) {
            continue;
        }

        for (uint16_t y = y0; y < y1; y++) {
            push_line_segment(tft, screen_w, screen_h, rect.x + segment_x,
                              rect.y + y, count, line);
        }
    }

    return true;
}

bool draw_scaled_mcu_rotated_ccw(TFT_eSPI* tft,
                                 uint16_t screen_w,
                                 uint16_t screen_h,
                                 const FitRect& rect,
                                 const uint16_t* src,
                                 uint16_t src_stride,
                                 uint16_t block_w,
                                 uint16_t block_h,
                                 uint16_t src_x,
                                 uint16_t src_y,
                                 uint16_t image_w,
                                 uint16_t logical_w,
                                 uint16_t logical_h) {
    uint16_t line[MAX_LINE_PIXELS];

    for (uint16_t col = 0; col < block_w; col++) {
        const uint16_t logical_y = image_w - 1 - (src_x + col);
        const uint16_t y0 = scaled_start(logical_y, rect.h, logical_h);
        const uint16_t y1 = scaled_end(logical_y, rect.h, logical_h);
        if (y1 <= y0) {
            continue;
        }

        bool has_segment = false;
        uint16_t segment_x = 0;
        uint16_t count = 0;

        for (uint16_t row = 0; row < block_h; row++) {
            const uint16_t logical_x = src_y + row;
            const uint16_t x0 = scaled_start(logical_x, rect.w, logical_w);
            const uint16_t x1 = scaled_end(logical_x, rect.w, logical_w);
            if (!append_scaled_run(line, count, has_segment, segment_x, x0, x1,
                                   src[row * src_stride + col])) {
                return false;
            }
        }

        if (!has_segment || count == 0) {
            continue;
        }

        for (uint16_t y = y0; y < y1; y++) {
            push_line_segment(tft, screen_w, screen_h, rect.x + segment_x,
                              rect.y + y, count, line);
        }
    }

    return true;
}
}

bool ImageUtils::is_jpg(const char* filename) {
    if (!filename) return false;
    std::string name = filename;
    for (auto &c : name) c = tolower(c);
    return (name.find(".jpg") != std::string::npos ||
            name.find(".jpeg") != std::string::npos);
}

bool ImageUtils::is_gif(const char* filename) {
    if (!filename) return false;
    std::string name = filename;
    for (auto &c : name) c = tolower(c);
    return name.find(".gif") != std::string::npos;
}

bool ImageUtils::validate_jpg_header(const uint8_t* data, size_t len) {
    // JPG files start with FFD8FF
    if (len < 3) return false;
    return (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF);
}

bool ImageUtils::validate_gif_header(const uint8_t* data, size_t len) {
    // GIF files start with "GIF87a" or "GIF89a"
    if (len < 6) return false;
    return (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
            (data[3] == '8') && (data[4] == '7' || data[4] == '9') &&
            data[5] == 'a');
}

bool ImageUtils::render_jpg_optimized(TFT_eSPI* tft, File& file, int x, int y) {
    if (!tft || !file) {
        return false;
    }

    SpiMutex::Guard spi_guard;
    if (!spi_guard.is_locked()) {
        Serial.println("[ImageUtils] ERROR: SPI bus busy while rendering JPG");
        return false;
    }

    if (!JpegDec.decodeSdFile(file)) {
        Serial.println("[ImageUtils] ERROR: Unsupported or corrupt JPG");
        return false;
    }

    const uint16_t screen_w = tft->width();
    const uint16_t screen_h = tft->height();
    const uint16_t image_w = JpegDec.width;
    const uint16_t image_h = JpegDec.height;

    const bool rotate_ccw = (screen_w > screen_h && image_h > image_w);
    const uint16_t logical_w = rotate_ccw ? image_h : image_w;
    const uint16_t logical_h = rotate_ccw ? image_w : image_h;
    const FitRect rect = fit_contain(logical_w, logical_h, screen_w, screen_h, x, y);

    Serial.printf("[ImageUtils] JPG %ux%u -> TFT %ux%u fit %ux%u at %d,%d%s\n",
                  image_w, image_h, screen_w, screen_h, rect.w, rect.h, rect.x, rect.y,
                  rotate_ccw ? " rotated CCW" : "");

    const bool old_swap = tft->getSwapBytes();
    tft->setSwapBytes(true);

    const uint16_t mcu_w = JpegDec.MCUWidth;
    const uint16_t mcu_h = JpegDec.MCUHeight;
    const uint16_t edge_w = edge_block_size(mcu_w, image_w);
    const uint16_t edge_h = edge_block_size(mcu_h, image_h);
    bool rendered = true;

    while (rendered && JpegDec.read()) {
        uint16_t* p_img = JpegDec.pImage;
        const int src_x = JpegDec.MCUx * mcu_w;
        const int src_y = JpegDec.MCUy * mcu_h;

        uint16_t block_w = (src_x + mcu_w <= image_w) ? mcu_w : edge_w;
        uint16_t block_h = (src_y + mcu_h <= image_h) ? mcu_h : edge_h;

        if (rotate_ccw) {
            rendered = draw_scaled_mcu_rotated_ccw(
                tft, screen_w, screen_h, rect, p_img, mcu_w, block_w, block_h,
                static_cast<uint16_t>(src_x), static_cast<uint16_t>(src_y),
                image_w, logical_w, logical_h);
            continue;
        }

        rendered = draw_scaled_mcu(
            tft, screen_w, screen_h, rect, p_img, mcu_w, block_w, block_h,
            static_cast<uint16_t>(src_x), static_cast<uint16_t>(src_y),
            logical_w, logical_h);
    }

    tft->setSwapBytes(old_swap);
    if (!rendered) {
        Serial.println("[ImageUtils] ERROR: JPG scaling buffer overflow");
    }
    return rendered;
}

bool ImageUtils::render_gif_frame(TFT_eSPI* tft, File& file, int x, int y) {
    // Placeholder for GIF frame rendering
    Serial.println("[ImageUtils] TODO: Implement GIF frame rendering");
    return false;
}

void ImageUtils::scale_dimensions(uint32_t src_w, uint32_t src_h,
                                 uint32_t max_w, uint32_t max_h,
                                 uint32_t& out_w, uint32_t& out_h) {
    // Calculate scaling to fit within max dimensions while maintaining aspect ratio
    float aspect = (float)src_w / src_h;
    float max_aspect = (float)max_w / max_h;

    if (aspect > max_aspect) {
        // Width is the limiting factor
        out_w = max_w;
        out_h = (uint32_t)(max_w / aspect);
    } else {
        // Height is the limiting factor
        out_h = max_h;
        out_w = (uint32_t)(max_h * aspect);
    }
}

bool ImageUtils::check_resolution(const char* filename, bool is_jpg) {
    // JPG: max 480x320
    // GIF: max 240x320
    // This would check the actual image dimensions when reading the header
    return true; // Placeholder
}
