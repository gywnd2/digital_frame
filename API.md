# API Documentation - CYD Digital Frame

## REST API Reference

### Base URL
```
http://<device-ip>/api
```

### Status Code Conventions
- `200 OK`: Request successful
- `400 Bad Request`: Missing or invalid parameters
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server-side error

---

## Endpoints

### 1. Device Status

#### Get System Status
```http
GET /api/status
```

**Response:**
```json
{
  "wifi_connected": true,
  "ip": "192.168.1.100",
  "sd_ready": true
}
```

| Field | Type | Description |
|-------|------|-------------|
| `wifi_connected` | boolean | WiFi connection status |
| `ip` | string | Device IP address |
| `sd_ready` | boolean | SD card ready status |

---

### 2. File Management

#### List All Images
```http
GET /api/files
```

**Response:**
```json
{
  "files": [
    {
      "name": "img_0001.jpg",
      "size": 45234
    },
    {
      "name": "img_0002.gif",
      "size": 123456
    }
  ]
}
```

#### Upload Image
```http
POST /api/upload
Content-Type: multipart/form-data

file=<binary data>
```

**Response:**
```json
{
  "status": "ok"
}
```

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `file` | File | Yes | JPG or GIF image file |

**Constraints:**
- JPG max: 480×320 pixels
- GIF max: 240×320 pixels
- Max upload size: 50 MB
- File auto-renamed to: `img_XXXX.{jpg,gif}`

#### Delete Single File
```http
POST /api/delete
Content-Type: application/x-www-form-urlencoded

file=img_0001.jpg
```

**Response:**
```json
{
  "status": "deleted"
}
```

#### Delete All Files
```http
POST /api/delete-all
```

**Response:**
```json
{
  "status": "deleted_all"
}
```

⚠️ **Warning**: This action cannot be undone.

---

### 3. Settings

#### Get Current Settings
```http
GET /api/settings
```

**Response:**
```json
{
  "ssid": "MyWiFiNetwork",
  "playback_mode": "sequential",
  "image_delay": 3000
}
```

#### Update Settings
```http
POST /api/settings
Content-Type: application/x-www-form-urlencoded

ssid=NewNetwork&password=newpass
```

**Response:**
```json
{
  "status": "saved"
}
```

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ssid` | string | Yes | WiFi network name |
| `password` | string | Yes | WiFi password |

---

### 4. Playback Control (Future)

#### Start Playback
```http
POST /api/playback/start
```

#### Pause Playback
```http
POST /api/playback/pause
```

#### Stop Playback
```http
POST /api/playback/stop
```

#### Next Image
```http
POST /api/playback/next
```

#### Previous Image
```http
POST /api/playback/previous
```

#### Set Playback Mode
```http
POST /api/playback/mode
Content-Type: application/x-www-form-urlencoded

mode=sequential
```

**Valid modes:** `sequential`, `random`

#### Set Image Delay
```http
POST /api/playback/delay
Content-Type: application/x-www-form-urlencoded

delay=3000
```

**Parameters:**
- `delay`: milliseconds (1000-30000)

---

## WebSocket API

### Connection
```
ws://<device-ip>:81
```

### Message Format
All messages are JSON objects:

```json
{
  "type": "message_type",
  "data": {}
}
```

### Server → Client Messages

#### Upload Progress
```json
{
  "type": "upload-progress",
  "data": {
    "percent": 50,
    "filename": "photo.jpg",
    "bytes_written": 1024000,
    "total_bytes": 2048000
  }
}
```

#### File List Updated
```json
{
  "type": "file-list-updated",
  "data": {
    "count": 5,
    "total_size": 5242880
  }
}
```

#### Device Status Update
```json
{
  "type": "status-update",
  "data": {
    "wifi_connected": true,
    "sd_ready": true,
    "free_space": 10485760
  }
}
```

#### Playback Status
```json
{
  "type": "playback-status",
  "data": {
    "playing": true,
    "current_image": "img_0001.jpg",
    "current_index": 0,
    "total_images": 5
  }
}
```

### Client → Server Messages

#### Request Status
```json
{
  "type": "request-status"
}
```

#### Request File List
```json
{
  "type": "request-files"
}
```

#### Playback Command
```json
{
  "type": "playback-command",
  "data": {
    "command": "play"
  }
}
```

**Valid commands:** `play`, `pause`, `stop`, `next`, `previous`

---

## HTTP Headers

### Request Headers
```
Host: <device-ip>
User-Agent: <client-info>
Content-Type: application/json | application/x-www-form-urlencoded | multipart/form-data
```

### Response Headers
```
Content-Type: application/json
Content-Length: <size>
Connection: keep-alive
```

---

## Error Responses

### Generic Error
```json
{
  "error": "Error message description",
  "code": 500
}
```

### Upload Validation Error
```json
{
  "error": "Invalid file format. Only JPG and GIF allowed.",
  "code": 400
}
```

### SD Card Error
```json
{
  "error": "SD card not ready",
  "code": 503
}
```

### Insufficient Space
```json
{
  "error": "Insufficient space on SD card (need 1MB, have 512KB)",
  "code": 507
}
```

---

## Usage Examples

### Python Example
```python
import requests
import json

BASE_URL = "http://192.168.1.100"

# Get device status
response = requests.get(f"{BASE_URL}/api/status")
status = response.json()
print(f"Connected: {status['wifi_connected']}")
print(f"IP: {status['ip']}")

# Upload image
with open("photo.jpg", "rb") as f:
    files = {"file": f}
    response = requests.post(f"{BASE_URL}/api/upload", files=files)
    print(response.json())

# Get file list
response = requests.get(f"{BASE_URL}/api/files")
files = response.json()
print(f"Images: {len(files['files'])}")

# Delete file
response = requests.post(
    f"{BASE_URL}/api/delete",
    data={"file": "img_0001.jpg"}
)
print(response.json())

# Update WiFi settings
response = requests.post(
    f"{BASE_URL}/api/settings",
    data={
        "ssid": "NewNetwork",
        "password": "newpassword"
    }
)
print(response.json())
```

### JavaScript Example (Fetch API)
```javascript
const BASE_URL = "http://192.168.1.100";

// Get device status
fetch(`${BASE_URL}/api/status`)
  .then(r => r.json())
  .then(data => {
    console.log("WiFi:", data.wifi_connected);
    console.log("IP:", data.ip);
  });

// Upload file
const formData = new FormData();
formData.append("file", fileInput.files[0]);

fetch(`${BASE_URL}/api/upload`, {
  method: "POST",
  body: formData
})
.then(r => r.json())
.then(data => console.log(data));

// Get file list
fetch(`${BASE_URL}/api/files`)
  .then(r => r.json())
  .then(data => {
    data.files.forEach(f => {
      console.log(`${f.name} (${f.size} bytes)`);
    });
  });

// Delete file
fetch(`${BASE_URL}/api/delete`, {
  method: "POST",
  headers: {
    "Content-Type": "application/x-www-form-urlencoded"
  },
  body: "file=img_0001.jpg"
})
.then(r => r.json())
.then(data => console.log(data));
```

### cURL Examples
```bash
# Get status
curl http://192.168.1.100/api/status

# Get file list
curl http://192.168.1.100/api/files

# Upload file
curl -F "file=@photo.jpg" http://192.168.1.100/api/upload

# Delete file
curl -X POST -d "file=img_0001.jpg" http://192.168.1.100/api/delete

# Delete all
curl -X POST http://192.168.1.100/api/delete-all

# Update WiFi
curl -X POST -d "ssid=MyNetwork&password=mypass" \
  http://192.168.1.100/api/settings
```

---

## Rate Limiting

- **Uploads**: Max 1 concurrent upload
- **Requests**: No hard limit, but recommended <100 req/sec
- **File Listing**: Cache for 5 seconds between refreshes

---

## Timeout Behavior

| Operation | Timeout |
|-----------|---------|
| Upload | 5 minutes |
| Download | 30 seconds |
| API Request | 10 seconds |
| WebSocket | Persistent |

---

## Data Validation

### File Names
- Accepted characters: `a-z`, `0-9`, `-`, `_`, `.`
- Max length: 255 characters
- Auto-renamed to: `img_XXXX.ext`

### Image Dimensions
- JPG: Scales down if > 480×320
- GIF: Rejects if > 240×320

### Image File Sizes
- Min: 1 KB
- Max: 50 MB
- Recommended: < 5 MB for best performance

---

## Concurrency & Thread Safety

- **Upload Queue**: Only 1 upload at a time
- **SPI Bus**: Mutex-protected (transparent to API)
- **File Access**: Read/write operations serialize on SD

---

## Changelog

### v1.0 (Initial Release)
- REST API with file upload/download
- File management (list, delete)
- WiFi configuration
- WebSocket for real-time updates
- Status monitoring

### Future
- Playback control API
- Advanced scheduling
- OTA firmware updates
- Cloud synchronization

---

For more details, see [README.md](README.md) and [CONFIGURATION.md](CONFIGURATION.md)
