# CYD Digital Frame Configuration

## System Configuration

### Hardware Pins
Refer to `include/cyd/User_Setup.h` for complete pin configuration.

**Critical Pins** (DO NOT MODIFY):
```
LCD SPI Bus:
  MOSI: GPIO13
  MISO: GPIO12
  SCLK: GPIO14
  CS: GPIO15
  DC: GPIO2

Touch CS: GPIO33
SD Card CS: GPIO5
Backlight: GPIO21
```

### Memory Layout
```
Flash: 4MB
  - Firmware: ~1.3MB
  - SPIFFS: ~2.7MB (for web UI assets)

SRAM: 520KB
  - LVGL Buffer: ~8KB
  - Task Stacks: ~20KB
  - Heap: ~400KB (available)
```

### Task Configuration
```
Core 0 (LVGL):
  Priority: 2
  Stack: 4KB
  Frequency: 240MHz

Core 1 (WiFi/Server):
  Priority: 1-2
  Stack: 4KB (WiFi), 8KB (Server)
  Frequency: 240MHz
```

## Performance Tuning

### SPI Frequencies
Modify in `include/cyd/User_Setup.h`:
```c
#define SPI_FREQUENCY  55000000    // Display (55 MHz max for ILI9341)
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000  // XPT2046 (2.5 MHz max)
```

For SD card speed, edit storage initialization.

### LVGL Optimization
Modify in `include/lv_conf.h`:
```c
#define LV_DEF_REFR_PERIOD  30     // 30ms refresh (33ms default)
#define LV_MEM_SIZE (128 * 1024U)  // Buffer size
```

## Debugging

### Serial Output
```
Baud: 115200
Format: N,8,1
Use platformio device monitor
```

### Build with Debug Info
```bash
pio run -e esp32dev_debug
```

### Memory Debugging
```cpp
Serial.printf("Free heap: %u bytes\n", esp_get_free_heap_size());
Serial.printf("Min heap: %u bytes\n", esp_get_minimum_free_heap_size());
```

## File System Layout

### SD Card `/images/`
```
/
├── images/
│   ├── img_0001.jpg
│   ├── img_0002.gif
│   └── ...
```

### SPIFFS `/spiffs/`
```
/
├── web/
│   ├── index.html
│   ├── css/
│   │   └── style.css
│   └── js/
│       └── app.js
├── settings.json
└── ...
```

## Customization

### Default WiFi Settings
Edit `src/network/NetworkManager.cpp`:
```cpp
#define WIFI_SSID "Your_SSID"
#define WIFI_PASS "Your_Password"
```

### Display Default Orientation
Edit `src/hardware/DisplayManager.cpp`:
```cpp
tft.setRotation(0);  // 0=Portrait, 1=Landscape, 2=Reverse Portrait, 3=Reverse Landscape
```

### Playback Default Settings
Edit `src/playback/PlaybackManager.cpp`:
```cpp
delay_ms = 3000;     // 3 seconds between images
mode = PLAYBACK_SEQUENTIAL;  // or PLAYBACK_RANDOM
```

## Dependencies Management

### Required Libraries
All libraries are specified in `platformio.ini`.

**Main Dependencies:**
- `bodmer/TFT_eSPI` - Display driver
- `lvgl/lvgl` - UI framework
- `links2004/WebSockets` - WebSocket support
- `mathieucarbou/AsyncTCP` - Async networking
- `marvinroger/ArduinoJson` - JSON parsing

### Library Versions
Pin specific versions in `platformio.ini` for stability:
```ini
lib_deps =
    bodmer/TFT_eSPI @ ^2.5.4
    lvgl/lvgl @ ^9.1.0
```

## Troubleshooting Build Issues

### "error: TFT_eSPI not found"
```bash
pio lib install "bodmer/TFT_eSPI"
```

### "error: lvgl.h not found"
```bash
pio lib install "lvgl/lvgl"
```

### Build Size Too Large
Edit `platformio.ini`:
```ini
build_flags =
    -Os  # Enable size optimization
```

### Serial Monitor Not Working
```bash
# List available ports
pio device list

# Connect to specific port
pio device monitor -p COM3
```

## Power Consumption

### Typical Operation
- **Display Active**: ~250 mA
- **WiFi Idle**: ~80 mA
- **WiFi Transmitting**: ~200 mA
- **Sleep (if implemented)**: ~30 mA

**Total Average**: ~300-350 mA

### Reducing Power
1. Reduce display brightness
2. Implement WiFi sleep when not uploading
3. Extend image delay
4. Use darker images

## Next Steps

1. **Test WiFi Connection** - Verify connectivity with default credentials
2. **Upload Sample Image** - Test upload system with small JPG
3. **Configure WiFi** - Set actual network credentials
4. **Add Images** - Upload collection of photos
5. **Adjust Playback** - Configure timing and order

---

For questions or issues, refer to README.md or check serial output for error messages.
