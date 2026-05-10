#include "../../include/network/NetworkManager.h"
#include "../../include/storage/StorageManager.h"
#include "../../include/hardware/DisplayManager.h"
#include "../../include/playback/PlaybackManager.h"
#include "../../include/hardware/SpiMutex.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <pgmspace.h>

extern DisplayManager g_display;
extern StorageManager g_storage;
extern PlaybackManager g_playback;

CydNetworkManager* CydNetworkManager::instance = nullptr;

namespace {
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>CYD Digital Frame</title>
  <link rel="stylesheet" href="/css/style.css">
</head>
<body>
  <main class="shell">
    <header>
      <h1>CYD Digital Frame</h1>
      <div class="status">
        <span id="wifi">WiFi: checking</span>
        <span id="sd">SD: checking</span>
      </div>
    </header>

    <section class="panel">
      <h2>Upload</h2>
      <label id="drop" class="drop">
        <input id="file" type="file" accept=".jpg,.jpeg,.gif,image/jpeg,image/gif" multiple>
        <strong>Drop JPG or GIF files here</strong>
        <span>or tap to choose files</span>
      </label>
      <div class="bar"><div id="bar"></div></div>
      <p id="progress">Idle</p>
    </section>

    <section class="panel grid">
      <div>
        <h2>Playback</h2>
        <label>Mode
          <select id="mode">
            <option value="sequential">Sequential</option>
            <option value="random">Random</option>
          </select>
        </label>
        <label>Delay
          <input id="delay" type="number" min="1" max="60" value="3"> seconds
        </label>
        <button id="savePlayback">Save Playback</button>
      </div>
      <div>
        <h2>Remote Sync</h2>
        <label>Image URL
          <input id="syncUrl" type="url" placeholder="https://example.com/image.jpg">
        </label>
        <button id="sync">Fetch Image</button>
      </div>
    </section>

    <section class="panel">
      <div class="row">
        <h2>Files</h2>
        <button id="refresh">Refresh</button>
        <button id="deleteAll" class="danger">Delete All</button>
      </div>
      <div id="files" class="files">Loading...</div>
    </section>
  </main>
  <div id="toast" class="toast" role="status" aria-live="polite"></div>
  <script src="/js/app.js"></script>
</body>
</html>
)rawliteral";

const char STYLE_CSS[] PROGMEM = R"rawliteral(
*{box-sizing:border-box}body{margin:0;background:#101418;color:#eef3f8;font-family:system-ui,-apple-system,Segoe UI,sans-serif}.shell{max-width:920px;margin:0 auto;padding:18px}header{display:flex;justify-content:space-between;gap:16px;align-items:center;margin-bottom:16px}h1{font-size:28px;margin:0}h2{font-size:18px;margin:0 0 14px}.status{display:flex;gap:8px;flex-wrap:wrap}.status span{border:1px solid #33424f;background:#18222b;padding:7px 10px;border-radius:6px}.panel{background:#182029;border:1px solid #2c3945;border-radius:8px;padding:16px;margin-bottom:14px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}.drop{display:flex;min-height:136px;border:2px dashed #4f91d9;border-radius:8px;align-items:center;justify-content:center;flex-direction:column;gap:6px;cursor:pointer;background:#121c25;transition:background .15s,border-color .15s,transform .12s}.drop.drag,.drop:active{background:#18304a;border-color:#75b7ff;transform:scale(.995)}.drop input{display:none}.bar{height:10px;background:#2a3540;border-radius:999px;overflow:hidden;margin-top:14px}.bar div{height:100%;width:0;background:#58c879;transition:width .15s}.row{display:flex;gap:10px;align-items:center}.row h2{margin-right:auto}button,select,input{font:inherit;border-radius:6px;border:1px solid #405160;background:#101820;color:#eef3f8;padding:8px 10px}button{background:#2368a8;border-color:#2f7fc8;cursor:pointer;box-shadow:0 2px 0 #15476f;transition:transform .1s,filter .12s,box-shadow .1s,opacity .12s}button:hover{filter:brightness(1.1)}button:active,button.pulse{transform:translateY(2px) scale(.98);box-shadow:0 0 0 #15476f}button.busy,button:disabled{opacity:.62;cursor:wait;filter:grayscale(.15)}button.danger{background:#963039;border-color:#bd4651;box-shadow:0 2px 0 #6c2028}.toast{position:fixed;left:50%;bottom:22px;z-index:20;max-width:calc(100% - 28px);transform:translate(-50%,16px);opacity:0;pointer-events:none;background:#081016;color:#fff;border:1px solid #395063;border-radius:8px;padding:12px 16px;box-shadow:0 10px 28px rgba(0,0,0,.4);transition:opacity .18s,transform .18s}.toast.show{opacity:1;transform:translate(-50%,0)}.toast.ok{border-color:#4fa66a}.toast.error{border-color:#d96066}label{display:block;margin:10px 0}.files{display:grid;gap:8px}.file{display:flex;gap:10px;align-items:center;border:1px solid #2d3945;border-radius:6px;padding:10px}.file span:first-child{flex:1;overflow-wrap:anywhere}.muted{color:#a9b5c0}@media(max-width:700px){header,.grid{display:block}.status{margin-top:12px}.row{flex-wrap:wrap}}
)rawliteral";

const char APP_JS[] PROGMEM = R"rawliteral(
const $=id=>document.getElementById(id);
let ws,toastTimer;
function setProgress(p,t){$('bar').style.width=p+'%';$('progress').textContent=t||p+'%'}
function toast(msg,kind='ok'){const el=$('toast');el.textContent=msg;el.className='toast show '+kind;clearTimeout(toastTimer);toastTimer=setTimeout(()=>el.className='toast',2300)}
function pulse(btn){if(!btn)return;btn.classList.remove('pulse');void btn.offsetWidth;btn.classList.add('pulse');setTimeout(()=>btn.classList.remove('pulse'),180)}
async function busy(btn,fn,ok){pulse(btn);btn.classList.add('busy');btn.disabled=true;try{await fn();if(ok)toast(ok)}catch(e){toast(e.message||'Failed','error')}finally{btn.disabled=false;btn.classList.remove('busy')}}
function supported(file){const n=file.name.toLowerCase();return n.endsWith('.jpg')||n.endsWith('.jpeg')||n.endsWith('.gif')||file.type==='image/jpeg'||file.type==='image/gif'}
function uploadError(xhr){try{const j=JSON.parse(xhr.responseText);return j.message||'Upload failed'}catch(e){return xhr.responseText||'Upload failed'}}
async function api(url,opt){const r=await fetch(url,opt);if(!r.ok){const text=await r.text();let msg=text||'Request failed';try{const j=JSON.parse(text);msg=j.message||msg}catch(e){}throw new Error(msg)}return r}
async function status(){const r=await fetch('/api/status');const j=await r.json();$('wifi').textContent='WiFi: '+(j.wifi_connected?j.ip:'disconnected');$('sd').textContent='SD: '+(j.sd_ready?'ready':'not ready')}
async function list(){const r=await fetch('/api/files');const j=await r.json();const root=$('files');root.innerHTML='';if(!j.files||!j.files.length){root.innerHTML='<p class="muted">No images uploaded yet.</p>';return}j.files.forEach(f=>{const d=document.createElement('div');d.className='file';d.innerHTML=`<span>${f.name}</span><span class="muted">${Math.round(f.size/1024)} KB</span><button class="danger">Delete</button>`;const b=d.querySelector('button');b.onclick=()=>busy(b,()=>del(f.name),'Deleted '+f.name);root.appendChild(d)})}
async function upload(file){if(!supported(file)){setProgress(0,'Only JPG and GIF files are supported: '+file.name);toast('Only JPG and GIF files are supported','error');return}const xhr=new XMLHttpRequest();xhr.upload.onprogress=e=>{if(e.lengthComputable)setProgress(Math.round(e.loaded*100/e.total),'Uploading '+file.name)};xhr.onload=()=>{if(xhr.status<300){setProgress(100,'Done');toast('Uploaded '+file.name)}else{const m=uploadError(xhr);setProgress(0,m);toast(m,'error')}list();status()};xhr.onerror=()=>{setProgress(0,'Upload failed');toast('Upload failed','error')};const fd=new FormData();fd.append('file',file);xhr.open('POST','/upload');xhr.send(fd)}
async function del(name){await api('/api/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'file='+encodeURIComponent(name)});await list()}
function wsOpen(){ws=new WebSocket('ws://'+location.host+'/ws');ws.onmessage=e=>{const m=JSON.parse(e.data);if(m.type==='upload-progress')setProgress(m.percent,'Uploading '+m.filename);if(m.type==='status')status();if(m.type==='files')list()};ws.onclose=()=>setTimeout(wsOpen,3000)}
document.addEventListener('DOMContentLoaded',()=>{const drop=$('drop'),file=$('file');drop.ondragover=e=>{e.preventDefault();drop.classList.add('drag')};drop.ondragleave=()=>drop.classList.remove('drag');drop.ondrop=e=>{e.preventDefault();drop.classList.remove('drag');pulse(drop);[...e.dataTransfer.files].forEach(upload)};file.onchange=e=>[...e.target.files].forEach(upload);$('refresh').onclick=e=>busy(e.currentTarget,list,'File list refreshed');$('deleteAll').onclick=e=>{if(confirm('Delete all images?'))busy(e.currentTarget,async()=>{await api('/api/delete-all',{method:'POST'});await list()},'Deleted all images')};$('savePlayback').onclick=e=>busy(e.currentTarget,()=>api('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'mode='+$('mode').value+'&delay='+$('delay').value}),'Playback settings saved');$('sync').onclick=e=>busy(e.currentTarget,()=>api('/api/sync',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent($('syncUrl').value)}),'Sync request sent');status();list();wsOpen();setInterval(status,5000)});
)rawliteral";

String json_error(const char* message) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = message;
    String response;
    serializeJson(doc, response);
    return response;
}

String extension_for_filename(const String& filename) {
    String lower = filename;
    lower.toLowerCase();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
        return "jpg";
    }
    if (lower.endsWith(".gif")) {
        return "gif";
    }
    return "";
}
}

void CydNetworkManager::wifi_event_handler(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!instance) return;

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("[WiFi] STA Start");
            instance->wifi_state = WIFI_CONNECTING;
            // Show connecting status immediately after display init
            if (instance && g_display.is_initialized()) {
                g_display.show_wifi_connecting(instance->ssid.c_str());
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi] STA Connected");
            instance->wifi_state = WIFI_CONNECTED;
            // Don't update display here to avoid LVGL thread conflicts
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            {
                instance->ip_address = WiFi.localIP().toString().c_str();
                instance->wifi_state = WIFI_CONNECTED;
                Serial.printf("[WiFi] Got IP: %s\n", instance->ip_address.c_str());
                // Update display only when fully connected
                if (instance && g_display.is_initialized()) {
                    g_display.show_wifi_connected(instance->ssid.c_str(), instance->ip_address.c_str());
                }
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            instance->wifi_state = WIFI_DISCONNECTED;
            Serial.println("[WiFi] Disconnected");
            // Don't update display here to avoid conflicts
            WiFi.reconnect();
            break;

        default:
            break;
    }
}

void CydNetworkManager::wifi_task_fn(void* arg) {
    if (!instance) return;

    // WiFi task runs on Core 1 - handles connection monitoring
    while (1) {
        instance->ws.cleanupClients();
        if (instance->wifi_state == WIFI_DISCONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void CydNetworkManager::websocket_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    (void)server;
    (void)arg;
    (void)data;
    (void)len;

    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WebSocket] Client %u connected\n", client->id());
        client->text("{\"type\":\"status\"}");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WebSocket] Client %u disconnected\n", client->id());
    }
}

CydNetworkManager::CydNetworkManager()
    : web_server(HTTP_PORT), ws("/ws"),
      wifi_state(WIFI_NOT_STARTED), server_started(false), wifi_task(nullptr) {
    instance = this;
}

CydNetworkManager::~CydNetworkManager() {
    instance = nullptr;
}

void CydNetworkManager::init(const char* ssid, const char* password) {
    Serial.println("[Network] Initializing...");

    this->ssid = ssid;
    this->password = password;

    // Initialize SPIFFS for settings storage
    if (!SPIFFS.begin()) {
        Serial.println("[Network] ERROR: SPIFFS mount failed");
    }

    // Set WiFi event handler
    WiFi.onEvent(wifi_event_handler);

    // Try to load saved settings
    std::string saved_ssid, saved_pass;
    if (load_settings(saved_ssid, saved_pass)) {
        this->ssid = saved_ssid;
        this->password = saved_pass;
        Serial.printf("[Network] Loaded settings: %s\n", this->ssid.c_str());
    }

    // Create WiFi task (Core 1)
    xTaskCreatePinnedToCore(
        wifi_task_fn,
        "WiFi",
        4096,
        nullptr,
        1,
        &wifi_task,
        1
    );

    connect_wifi(this->ssid.c_str(), this->password.c_str());
}

bool CydNetworkManager::connect_wifi(const char* ssid, const char* pass) {
    Serial.printf("[Network] Connecting to %s...\n", ssid);
    g_display.show_wifi_connecting(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    // Wait for connection with timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        ip_address = WiFi.localIP().toString().c_str();
        wifi_state = WIFI_CONNECTED;
        Serial.printf("[Network] Connected! IP: %s\n", ip_address.c_str());
        g_display.show_wifi_connected(ssid, ip_address.c_str());
        return true;
    } else {
        wifi_state = WIFI_DISCONNECTED;
        Serial.println("[Network] Connection failed");
        return false;
    }
}

void CydNetworkManager::disconnect_wifi() {
    WiFi.disconnect(true);
    wifi_state = WIFI_DISCONNECTED;
    Serial.println("[Network] Disconnected");
}

void CydNetworkManager::start_server() {
    if (server_started) return;

    Serial.println("[Network] Starting web server...");

    ws.onEvent(websocket_event_handler);
    web_server.addHandler(&ws);

    web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    web_server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    web_server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", STYLE_CSS);
    });
    web_server.on("/js/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "application/javascript", APP_JS);
    });

    // REST API endpoints
    web_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!instance) {
            request->send(500, "application/json", json_error("Server error"));
            return;
        }

        JsonDocument doc;
        doc["wifi_connected"] = (instance->wifi_state == WIFI_CONNECTED);
        doc["ip"] = instance->ip_address;
        doc["sd_ready"] = g_storage.is_sd_ready();
        doc["free_bytes"] = g_storage.get_free_space();
        doc["playback_mode"] = (g_playback.get_mode() == PLAYBACK_RANDOM) ? "random" : "sequential";
        doc["delay_ms"] = g_playback.get_delay();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    struct UploadState {
        File file;
        std::string path;
        String filename;
        bool active = false;
        bool failed = false;
        bool playback_paused = false;
        String error;
        size_t written = 0;
    };

    static UploadState upload;

    auto upload_response = [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        if (upload.failed) {
            Serial.printf("[Network] Upload failed: %s - %s\n",
                          upload.filename.c_str(), upload.error.c_str());
            doc["status"] = "error";
            doc["message"] = upload.error;
            if (upload.file) {
                SpiMutex::Guard spi_guard;
                if (spi_guard.is_locked()) {
                    upload.file.close();
                }
            }
            if (!upload.path.empty()) {
                g_storage.cancel_upload(upload.path.c_str());
            }
            if (upload.playback_paused) {
                g_playback.resume();
            }
            upload = UploadState();
            String response;
            serializeJson(doc, response);
            request->send(400, "application/json", response);
            return;
        }

        doc["status"] = "ok";
        doc["path"] = upload.path;
        doc["bytes"] = upload.written;
        upload = UploadState();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    };

    auto upload_stream = [](AsyncWebServerRequest *request, const String& filename,
                            size_t index, uint8_t *data, size_t len, bool final) {
        if (!instance) return;

        if (index == 0) {
            Serial.printf("[Network] Upload start: %s (%u bytes request)\n",
                          filename.c_str(), static_cast<unsigned>(request->contentLength()));

            upload = UploadState();
            upload.active = true;
            upload.filename = filename;

            if (!g_storage.is_sd_ready()) {
                upload.failed = true;
                upload.error = "SD card is not ready";
                return;
            }

            String ext = extension_for_filename(filename);
            if (ext.isEmpty()) {
                upload.failed = true;
                upload.error = "Only JPG and GIF files are supported";
                Serial.printf("[Network] Rejecting unsupported upload: %s\n", filename.c_str());
                return;
            }

            if (!g_storage.has_sufficient_space(request->contentLength())) {
                upload.failed = true;
                upload.error = "Insufficient SD free space";
                return;
            }

            upload.playback_paused = g_playback.is_playing();
            g_playback.pause();
            upload.file = g_storage.open_upload_file(ext.c_str(), upload.path);
            if (!upload.file) {
                upload.failed = true;
                upload.error = "Failed to create upload file";
                if (upload.playback_paused) {
                    g_playback.resume();
                }
                return;
            }
        }

        if (upload.failed || !upload.file) {
            return;
        }

        {
            SpiMutex::Guard spi_guard;
            if (!spi_guard.is_locked()) {
                upload.failed = true;
                upload.error = "SPI bus busy";
                return;
            }

            size_t written = upload.file.write(data, len);
            if (written != len) {
                upload.failed = true;
                upload.error = "SD write failed";
                return;
            }
            upload.written += written;
        }

        size_t total = request->contentLength();
        int progress = total > 0 ? static_cast<int>(((index + len) * 100) / total) : 0;
        progress = constrain(progress, 0, 100);
        instance->broadcast_upload_progress(progress, filename.c_str());
        g_display.show_upload_progress(progress, filename.c_str());

        if (final) {
            {
                SpiMutex::Guard spi_guard;
                if (spi_guard.is_locked()) {
                    upload.file.close();
                } else {
                    upload.failed = true;
                    upload.error = "SPI bus busy while closing upload";
                }
            }

            if (!upload.failed) {
                Serial.printf("[Network] Upload complete: %s -> %s\n",
                              filename.c_str(), upload.path.c_str());
                g_storage.finalize_upload(upload.path.c_str());
                instance->broadcast_upload_progress(100, filename.c_str());
                g_display.show_upload_complete();
                instance->broadcast_file_list();
                g_playback.reload_images();
                if (upload.playback_paused) {
                    g_playback.resume();
                } else if (!g_playback.is_playing()) {
                    g_playback.start();
                }
            }
        }
    };

    web_server.on("/upload", HTTP_POST, upload_response, upload_stream);
    web_server.on("/upload/", HTTP_POST, upload_response, upload_stream);
    web_server.on("/api/upload", HTTP_POST, upload_response, upload_stream);

    // File list endpoint
    web_server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
        auto files = g_storage.list_images();

        JsonDocument doc;
        JsonArray arr = doc["files"].to<JsonArray>();

        for (const auto& file : files) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"] = file.name;
            obj["size"] = file.size;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Delete file endpoint
    web_server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        const AsyncWebParameter *param = request->getParam("file", true);
        if (!param) {
            param = request->getParam("file", false);
        }
        if (!param) {
            request->send(400, "application/json", json_error("Missing file parameter"));
            return;
        }

        String filename = param->value();
        filename.replace("\\", "/");
        int slash = filename.lastIndexOf('/');
        if (slash >= 0) {
            filename = filename.substring(slash + 1);
        }

        char path[256];
        snprintf(path, sizeof(path), "/images/%s", filename.c_str());

        bool success = g_storage.delete_file(path);
        if (success) {
            request->send(200, "application/json", "{\"status\":\"deleted\"}");
            if (instance) {
                instance->broadcast_file_list();
            }
        } else {
            request->send(500, "application/json", json_error("Delete failed"));
        }
    });

    // Delete all endpoint
    web_server.on("/api/delete-all", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = g_storage.delete_all_images();
        if (success) {
            request->send(200, "application/json", "{\"status\":\"deleted_all\"}");
            if (instance) {
                instance->broadcast_file_list();
            }
        } else {
            request->send(500, "application/json", json_error("Delete failed"));
        }
    });

    // Settings endpoint
    web_server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["ssid"] = instance->ssid;
        doc["mode"] = (g_playback.get_mode() == PLAYBACK_RANDOM) ? "random" : "sequential";
        doc["delay_ms"] = g_playback.get_delay();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    web_server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        const AsyncWebParameter *mode_param = request->getParam("mode", true);
        const AsyncWebParameter *delay_param = request->getParam("delay", true);
        const AsyncWebParameter *ssid_param = request->getParam("ssid", true);
        const AsyncWebParameter *pass_param = request->getParam("password", true);

        if (mode_param) {
            String mode = mode_param->value();
            mode.toLowerCase();
            g_playback.set_mode(mode == "random" ? PLAYBACK_RANDOM : PLAYBACK_SEQUENTIAL);
        }

        if (delay_param) {
            uint32_t seconds = static_cast<uint32_t>(delay_param->value().toInt());
            seconds = constrain(seconds, 1U, 60U);
            g_playback.set_delay(seconds * 1000U);
        }

        if (ssid_param && pass_param) {
            std::string new_ssid = ssid_param->value().c_str();
            std::string new_pass = pass_param->value().c_str();

            if (instance->save_settings(new_ssid.c_str(), new_pass.c_str())) {
                request->send(200, "application/json", "{\"status\":\"saved\"}");
                instance->connect_wifi(new_ssid.c_str(), new_pass.c_str());
                return;
            }

            request->send(500, "application/json", json_error("WiFi settings save failed"));
            return;
        }

        request->send(200, "application/json", "{\"status\":\"saved\"}");
    });

    web_server.on("/api/sync", HTTP_POST, [](AsyncWebServerRequest *request) {
        const AsyncWebParameter *param = request->getParam("url", true);
        if (!param || param->value().isEmpty()) {
            request->send(400, "application/json", json_error("Missing url parameter"));
            return;
        }

        String url = param->value();
        String ext = extension_for_filename(url);
        if (ext.isEmpty()) {
            request->send(400, "application/json", json_error("URL must end with .jpg, .jpeg, or .gif"));
            return;
        }

        String *task_url = new String(url);
        xTaskCreatePinnedToCore(
            [](void *arg) {
                String *url_ptr = static_cast<String*>(arg);
                String url = *url_ptr;
                delete url_ptr;

                String ext = extension_for_filename(url);
                HTTPClient http;
                WiFiClient plain_client;
                WiFiClientSecure secure_client;
                secure_client.setInsecure();

                bool secure = url.startsWith("https://");
                bool begun = secure ? http.begin(secure_client, url) : http.begin(plain_client, url);
                if (!begun) {
                    Serial.println("[Sync] Failed to begin HTTP request");
                    vTaskDelete(nullptr);
                    return;
                }

                int code = http.GET();
                if (code != HTTP_CODE_OK) {
                    Serial.printf("[Sync] HTTP GET failed: %d\n", code);
                    http.end();
                    vTaskDelete(nullptr);
                    return;
                }

                int len = http.getSize();
                if (len > 0 && !g_storage.has_sufficient_space(static_cast<uint64_t>(len))) {
                    Serial.println("[Sync] Insufficient SD space");
                    http.end();
                    vTaskDelete(nullptr);
                    return;
                }

                std::string path;
                File file = g_storage.open_upload_file(ext.c_str(), path);
                if (!file) {
                    Serial.println("[Sync] Failed to open destination file");
                    http.end();
                    vTaskDelete(nullptr);
                    return;
                }

                uint8_t buffer[1024];
                WiFiClient *stream = http.getStreamPtr();
                bool failed = false;
                size_t total = 0;
                while (http.connected() && (len < 0 || total < static_cast<size_t>(len))) {
                    size_t available = stream->available();
                    if (!available) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                        continue;
                    }

                    size_t to_read = min(available, sizeof(buffer));
                    int read_len = stream->readBytes(buffer, to_read);
                    if (read_len <= 0) {
                        failed = true;
                        break;
                    }

                    {
                        SpiMutex::Guard spi_guard;
                        if (!spi_guard.is_locked() ||
                            file.write(buffer, static_cast<size_t>(read_len)) != static_cast<size_t>(read_len)) {
                            failed = true;
                            break;
                        }
                    }
                    total += static_cast<size_t>(read_len);
                }

                {
                    SpiMutex::Guard spi_guard;
                    if (spi_guard.is_locked()) {
                        file.close();
                    }
                }

                http.end();

                if (failed) {
                    g_storage.cancel_upload(path.c_str());
                    Serial.println("[Sync] Failed, partial file removed");
                } else {
                    g_storage.finalize_upload(path.c_str());
                    Serial.printf("[Sync] Completed: %s (%u bytes)\n", path.c_str(), static_cast<unsigned>(total));
                    if (instance) {
                        instance->broadcast_file_list();
                    }
                }
                vTaskDelete(nullptr);
            },
            "ImageSync",
            8192,
            task_url,
            1,
            nullptr,
            1
        );

        request->send(202, "application/json", "{\"status\":\"sync_started\"}");
    });

    web_server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(204);
            return;
        }
        request->send(404, "application/json", json_error("Not found"));
    });

    web_server.begin();
    server_started = true;
    Serial.println("[Network] Web server started on port 80");
}

void CydNetworkManager::stop_server() {
    if (server_started) {
        web_server.end();
        server_started = false;
        Serial.println("[Network] Web server stopped");
    }
}

void CydNetworkManager::broadcast_upload_progress(int percent, const char* filename) {
    Serial.printf("[Network] Upload progress: %d%% - %s\n", percent, filename);

    JsonDocument doc;
    doc["type"] = "upload-progress";
    doc["percent"] = percent;
    doc["filename"] = filename;
    String payload;
    serializeJson(doc, payload);
    ws.textAll(payload);
}

void CydNetworkManager::broadcast_device_status(const char* status) {
    Serial.printf("[Network] Device status: %s\n", status);

    JsonDocument doc;
    doc["type"] = "device-status";
    doc["status"] = status;
    String payload;
    serializeJson(doc, payload);
    ws.textAll(payload);
}

void CydNetworkManager::broadcast_file_list() {
    Serial.println("[Network] Broadcasting file list update");
    ws.textAll("{\"type\":\"files\"}");
}

bool CydNetworkManager::save_settings(const char* ssid, const char* pass) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = pass;

    File f = SPIFFS.open("/settings.json", "w");
    if (!f) {
        Serial.println("[Network] Failed to save settings");
        return false;
    }

    serializeJson(doc, f);
    f.close();

    Serial.println("[Network] Settings saved");
    return true;
}

bool CydNetworkManager::load_settings(std::string& out_ssid, std::string& out_pass) {
    if (!SPIFFS.exists("/settings.json")) {
        return false;
    }

    File f = SPIFFS.open("/settings.json", "r");
    if (!f) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        Serial.println("[Network] Failed to load settings");
        return false;
    }

    if (doc["ssid"].is<const char*>() && doc["password"].is<const char*>()) {
        out_ssid = doc["ssid"].as<const char*>();
        out_pass = doc["password"].as<const char*>();
        return true;
    }

    return false;
}
