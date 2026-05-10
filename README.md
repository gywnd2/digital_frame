# CYD Digital Frame Firmware

A full-featured ESP32 digital photo frame system for the Cheap Yellow Display (CYD) with WiFi connectivity, web UI, and advanced image management.

## Features

### ✅ Core Functionality
- **WiFi Connectivity**: STA mode with automatic IP display
- **File Upload System**: HTTP/REST API with direct SD streaming (no RAM buffering)
- **Image Playback**: Sequential/Random modes with configurable delay
- **Web UI**: Full-featured responsive interface for upload, settings, and management
- **REST API**: Complete API for external clients and automation
- **WebSocket**: Real-time progress updates and device status

### 📱 Image Formats
- **JPG/JPEG**: Max 480x320, auto-scaling if larger
- **GIF**: Max 240x320, frame-by-frame rendering with internal timing

### 🏗️ Architecture
- **Dual-Core Design**:
  - Core 0: LVGL UI + Image rendering
  - Core 1: WiFi + Web server + Upload handling
- **SPI Synchronization**: Global mutex for thread-safe access to LCD, Touch, and SD
- **No Full-Frame Buffering**: Optimized for slow SD cards and limited RAM

## Hardware Setup

### Board
- **Microcontroller**: ESP32-WROOM-32 (no PSRAM)
- **Display**: ILI9341 (240×320) via SPI
- **Touch**: XPT2046 via SPI
- **Storage**: microSD card via SPI
- **SPI Bus**: SHARED (managed via mutex)

### Pin Configuration
```
TFT Display (ILI9341)
├─ MOSI: GPIO13
├─ MISO: GPIO12
├─ SCLK: GPIO14
├─ CS: GPIO15
├─ DC: GPIO2
└─ BL: GPIO21

Touch (XPT2046)
└─ CS: GPIO33

microSD Card
└─ CS: GPIO5
```

## Project Structure

```
digital_frame/
├── include/
│   ├── hardware/
│   │   ├── SpiMutex.h           # SPI synchronization
│   │   └── DisplayManager.h      # LVGL + TFT management
│   ├── storage/
│   │   └── StorageManager.h      # SD card management
│   ├── network/
│   │   └── NetworkManager.h      # WiFi + Web server
│   ├── playback/
│   │   ├── PlaybackManager.h     # Image playback control
│   │   └── ImageUtils.h          # Image utilities
│   ├── app/
│   │   └── Application.h         # Main application coordinator
│   ├── lv_conf.h                 # LVGL configuration
│   └── cyd/
│       └── User_Setup.h          # TFT_eSPI configuration
├── src/
│   ├── hardware/
│   ├── storage/
│   ├── network/
│   ├── playback/
│   ├── app/
│   └── main.cpp
├── web/
│   ├── index.html               # Web UI
│   ├── css/
│   │   └── style.css
│   └── js/
│       └── app.js
├── platformio.ini
└── custom.csv
```

## Installation & Setup

### 1. Hardware Requirements
- ESP32 development board with integrated CYD
- microSD card (Class 2 or higher, any capacity)
- USB cable for programming

### 2. Software Setup

```bash
# Install PlatformIO (if not already installed)
pip install platformio

# Clone this repository
git clone <repo-url>
cd digital_frame

# Install dependencies
pio pkg install

# Build firmware
pio run

# Upload to board
pio run -t upload

# View serial output
pio device monitor
```

### 3. Initial Configuration

1. **Power on the device** - Display shows "Please Insert SD Card!" if not present
2. **WiFi Connection**:
   - Default SSID: `CYD_Frame`
   - Default Password: `password`
   - Access Web UI at device IP address
3. **Change WiFi Settings**:
   - Go to WiFi Settings section in Web UI
   - Enter SSID and password
   - Click "Save & Connect"

## Web UI Features

### 📤 Upload Images
- Drag & drop files or click to select
- Supports JPG and GIF formats
- Real-time upload progress
- Auto-naming with sequential numbering (img_0001.jpg, etc.)

### ▶️ Playback Controls
- **Play**: Start slideshow
- **Pause**: Pause/Resume
- **Stop**: Stop and clear screen
- **Playback Mode**: Sequential or Random
- **Image Delay**: 1-30 seconds (configurable)

### 📁 Image Library
- List all uploaded images with sizes
- Delete individual images
- Delete all images at once
- Real-time file count

### ⚙️ Settings
- WiFi network configuration
- Playback mode selection
- Image delay configuration
- System information display

## REST API Endpoints

### Status & Info
```
GET /api/status
Response: { wifi_connected, ip, sd_ready }

GET /api/files
Response: { files: [{name, size}, ...] }
```

### Upload
```
POST /api/upload
Body: multipart/form-data (file)
Returns: { status: "ok" }
```

### File Management
```
POST /api/delete?file=img_0001.jpg
POST /api/delete-all
```

### Settings
```
GET /api/settings
Returns: { ssid, ... }

POST /api/settings
Body: ssid=name&password=pass
```

## WebSocket API

Real-time events via WebSocket on port 81:

```javascript
{
  "type": "upload-progress",
  "percent": 50,
  "filename": "photo.jpg"
}

{
  "type": "file-list-updated"
}

{
  "type": "status-update"
}
```

## Image Handling

### Upload Rules
- **Storage**: `/images` directory on SD card
- **Auto-rename**: `img_0001.jpg`, `img_0002.gif`, etc.
- **Validation**: JPG/GIF headers checked before write
- **Streaming**: Chunked write (512-2048 bytes) directly to SD
- **Space Check**: Verifies sufficient free space before upload

### Format Constraints
| Format | Max Resolution | Scaling Policy |
|--------|---|---|
| JPG | 480×320 | Auto-scale if larger |
| GIF | 240×320 | Reject if exceeded |

### Resolution Handling
- JPG files exceeding 480×320 are automatically scaled down
- GIF files exceeding 240×320 are rejected
- Aspect ratio is preserved during scaling

## Error Handling

### Upload Interruption
- Partial files are automatically deleted
- Retry is supported with same filename
- Progress tracking via WebSocket

### Network Issues
- WiFi auto-reconnect on disconnect
- Graceful degradation without internet
- Device continues to play images locally

### SD Card Issues
- Full boot-time detection
- Error message displayed if not present
- File integrity checks before rendering

## Performance Optimization

### Memory
- No full-frame buffering in RAM
- Streaming image decoding
- LVGL buffer: 4096 pixels (~8KB)

### SD Card
- Optimized for Class 2 cards
- Chunked writing reduces latency
- Mutex prevents simultaneous access

### Display
- SPI frequency: 55 MHz (LCD), 25 MHz (SD), 2.5 MHz (Touch)
- LVGL refresh: 30 ms default
- Image rendering: Line-by-line where possible

## Concurrency Model

### FreeRTOS Tasks

**Core 0 (UI & Display)**
- LVGL update task (2KB priority)
- Image playback rendering
- Touch input handling (when implemented)

**Core 1 (Network)**
- WiFi management task
- HTTP server (AsyncWebServer)
- WebSocket server
- Upload handling

### Synchronization
- **SPI Mutex**: Protects all SPI device access
- **RAII Guard**: `SpiMutex::Guard` auto-locks/unlocks
- **No Blocking Calls**: UI thread remains responsive

## Advanced Features

### OTA Image Sync
- Device can fetch images from remote HTTP server
- Scheduled sync capability
- Auto-organize and manage collection

### WebSocket Monitoring
- Real-time upload progress
- Device status updates
- File list changes notification

### Rest API Integration
- Python scripts for automation
- Scheduled uploads
- Remote file management

## Troubleshooting

### Device Shows "Please Insert SD Card!"
1. Check SD card is properly inserted
2. Try different microSD card (some cheap ones may not work)
3. Format SD card with FAT32
4. Check serial output for detailed error messages

### WiFi Not Connecting
1. Verify SSID and password are correct
2. Check if router is within range
3. Try default credentials: `CYD_Frame` / `password`
4. Reset WiFi settings via Web UI

### Images Not Displaying
1. Verify image format (JPG or GIF)
2. Check image resolution (not exceeding max)
3. Ensure sufficient SD free space (>1MB)
4. Check upload completed successfully (100%)

### Slow Upload Speed
1. Use higher quality microSD card
2. Check USB cable (should be < 2m)
3. Reduce simultaneous connections
4. Close browser developer tools (reduces CPU)

### Touch Not Working
- Touch input not yet implemented
- Can use Web UI for control

## Development Notes

### Adding Features
1. Create new module in appropriate directory
2. Add header in `include/`
3. Add implementation in `src/`
4. Update `Application.h/cpp` to initialize
5. Test via Web UI or REST API

### Extending Image Support
1. Add format detection in `ImageUtils`
2. Implement rendering function
3. Update validation and scaling logic
4. Test with sample images

### Performance Tuning
1. Adjust LVGL refresh rate in `lv_conf.h`
2. Modify SPI frequencies in `User_Setup.h`
3. Change task priorities in task creation
4. Profile memory with `esp_get_free_heap_size()`

## Known Limitations

- ❌ No PSRAM: Limits full-frame buffering options
- ❌ Slow SD Card: Requires optimization for Class 2 cards
- ❌ Limited Flash: ~1.3MB available for firmware
- ❌ Single SPI Bus: All peripherals must be multiplexed
- ⚠️ Touch Input: Not yet implemented
- ⚠️ Image Editing: No in-device editing capability

## Future Enhancements

- [ ] Optimized JPG/GIF rendering with streaming decoder
- [ ] Touch input gesture recognition
- [ ] Image transitions and effects
- [ ] Scheduled playback (time-based)
- [ ] MQTT integration for smart home
- [ ] Bluetooth connectivity
- [ ] SD card auto-mount retry
- [ ] Battery support (for portable use)

## License

MIT License - See LICENSE file for details

## Credits

- **LVGL**: https://github.com/lvgl/lvgl
- **TFT_eSPI**: https://github.com/Bodmer/TFT_eSPI
- **Arduino Framework**: https://github.com/espressif/arduino-esp32
- **AsyncWebServer**: https://github.com/mathieucarbou/AsyncTCP

---

**Created for ESP32 + CYD Hardware**
*Optimized for reliability and simplicity*
