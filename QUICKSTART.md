# Quick Start Guide - CYD Digital Frame

## 5-Minute Setup

### Step 1: Hardware Check
- [ ] ESP32 board powered on
- [ ] ILI9341 display connected
- [ ] microSD card inserted
- [ ] USB cable connected for programming

### Step 2: Build & Upload
```bash
# Navigate to project directory
cd digital_frame

# Build firmware
platformio run

# Upload to board (select COM port if prompted)
platformio run -t upload

# Monitor output
platformio device monitor
```

**Expected Output:**
```
===== CYD Digital Frame =====
Initializing system...

[SPI Mutex] Initialized
[Display] Initializing TFT...
[Storage] Initializing SD card...
[Network] Initializing...
[Playback] Initializing...

[App] System ready!
```

### Step 3: First Connection
1. Open web browser
2. Find device IP from serial output (e.g., `192.168.1.100`)
3. Navigate to: `http://192.168.1.100`
4. You should see the CYD Digital Frame UI

### Step 4: Upload First Image
1. Drag & drop a JPG image onto upload area
2. Or click to select file
3. Watch progress bar fill
4. Image appears in "Image Library" section

### Step 5: Start Playback
1. Click "Play" button
2. Image displays on screen
3. Automatically advances to next image after delay

## Common Issues & Quick Fixes

### "Please Insert SD Card!" Error
- **Problem**: SD card not detected
- **Fix**:
  ```
  1. Power off device
  2. Check SD card is fully inserted
  3. Try different microSD card
  4. Format SD with FAT32
  5. Power on and restart
  ```

### Can't Access Web UI
- **Problem**: Device IP not showing/incorrect
- **Fix**:
  ```
  1. Check serial monitor for IP address
  2. Ensure WiFi is connected (green badge)
  3. Try http://192.168.1.1 and scan network
  4. Restart browser
  5. Check firewall settings
  ```

### WiFi Won't Connect
- **Problem**: Wrong credentials or out of range
- **Fix**:
  ```
  1. Try default: SSID="CYD_Frame", Pass="password"
  2. Move device closer to router
  3. Check router is broadcasting 2.4GHz (ESP32 limitation)
  4. Update WiFi settings in Web UI
  5. Check serial output for errors
  ```

### Upload Fails
- **Problem**: File not uploading or stopping midway
- **Fix**:
  ```
  1. Try smaller image (<1MB)
  2. Ensure SD has sufficient space (>1MB free)
  3. Check image format (JPG or GIF only)
  4. Try different USB cable
  5. Check if file already exists
  ```

### Images Not Displaying
- **Problem**: Upload successful but image won't show
- **Fix**:
  ```
  1. Verify image format (JPG or GIF)
  2. Check resolution (not too large)
  3. Try different image
  4. Delete and re-upload
  5. Check serial output for rendering errors
  ```

## Using Web UI

### Upload Images
```
1. Click upload area or select files
2. Supports JPG and GIF
3. Images auto-renamed: img_0001.jpg, img_0002.gif
4. Drag & drop supported
```

### Control Playback
```
Buttons:
- Play: Start slideshow
- Pause: Pause/Resume
- Stop: Stop and clear

Settings:
- Playback Mode: Sequential or Random
- Image Delay: 1-30 seconds
```

### Manage Files
```
1. Click "Refresh" to reload file list
2. "Delete" button removes individual image
3. "Delete All" clears image library
4. File sizes shown for reference
```

### WiFi Settings
```
1. Enter SSID (network name)
2. Enter Password
3. Click "Save & Connect"
4. Device reconnects to new network
5. Watch status badge for confirmation
```

## Command Line (Advanced)

### Monitor Device Status
```bash
platformio device monitor
# Ctrl+C to exit
```

### Upload with Different Port
```bash
platformio run -t upload -p COM4
```

### Build Only (No Upload)
```bash
platformio run
# Binary in .pio/build/esp32dev/firmware.bin
```

### Clean Build
```bash
platformio run --target clean
platformio run
```

## File System Access (Advanced)

### View Uploaded Images via REST API
```bash
curl http://192.168.1.100/api/files
```

**Response:**
```json
{
  "files": [
    {"name": "img_0001.jpg", "size": 45234},
    {"name": "img_0002.gif", "size": 123456}
  ]
}
```

### Delete Image via REST API
```bash
curl -X POST "http://192.168.1.100/api/delete?file=img_0001.jpg"
```

### Get Device Status
```bash
curl http://192.168.1.100/api/status
```

**Response:**
```json
{
  "wifi_connected": true,
  "ip": "192.168.1.100",
  "sd_ready": true
}
```

## Tips & Tricks

### Optimize Image Performance
- **JPG**: Pre-resize to max 480×320 pixels (optional, device auto-scales)
- **GIF**: Keep under 240×320 pixels for best performance
- **File Size**: Smaller files (~500KB-2MB) upload faster

### Better Image Quality
- Use quality JPG (85-95 quality)
- Avoid highly compressed GIFs
- Keep consistent aspect ratio with display

### Faster Upload
- Use 5GHz nearby for faster download to router
- Close background applications
- Minimize browser tabs
- Use wired connection if router has Ethernet

### Improve Reliability
- Keep microSD card in good condition
- Avoid removing power during upload
- Use high-quality USB cable
- Don't update WiFi too frequently

## Next Steps

After basic setup:

1. **Customize WiFi**: Set your actual network credentials
2. **Build Image Collection**: Upload 10+ images
3. **Adjust Timing**: Set preferred image delay
4. **Explore REST API**: Try commands via curl or Postman
5. **Review CONFIGURATION.md**: Advanced tweaking options

## Getting Help

### Check Serial Monitor
```bash
platformio device monitor

# Look for:
# - SPI Mutex initialization
# - Display/LVGL setup
# - WiFi connection status
# - Storage/SD messages
```

### Common Error Messages
| Error | Cause | Solution |
|-------|-------|----------|
| `SD initialization failed` | SD card issue | Reinsert or format |
| `Could not acquire SPI mutex` | SPI contention | Retry operation |
| `WiFi connection timeout` | Network unreachable | Move closer to router |
| `Upload failed` | SD space or file issue | Check free space |

### Additional Resources
- See README.md for full documentation
- Check CONFIGURATION.md for advanced settings
- Review source code comments for implementation details

---

**You're all set!** Enjoy your CYD Digital Frame! 🎉
