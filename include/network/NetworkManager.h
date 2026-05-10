#ifndef __NETWORK_MANAGER_H__
#define __NETWORK_MANAGER_H__

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <string>

#define WIFI_SSID "SSID"      // Configure via web interface
#define WIFI_PASS "PW"
#define HTTP_PORT 80
#define UPLOAD_TIMEOUT 300000      // 5 minutes
#define MAX_UPLOAD_SIZE (50 * 1024 * 1024) // 50MB

enum WiFiState {
    WIFI_NOT_STARTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_ERROR
};

class CydNetworkManager {
private:
    AsyncWebServer web_server;
    AsyncWebSocket ws;
    WiFiState wifi_state;
    std::string ssid;
    std::string password;
    std::string ip_address;
    TaskHandle_t wifi_task;
    bool server_started;

    static CydNetworkManager* instance;
    static void wifi_event_handler(WiFiEvent_t event, WiFiEventInfo_t info);
    static void wifi_task_fn(void* arg);
    static void websocket_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                        AwsEventType type, void *arg, uint8_t *data, size_t len);

public:
    CydNetworkManager();
    ~CydNetworkManager();

    void init(const char* ssid = WIFI_SSID, const char* password = WIFI_PASS);
    void start_server();
    void stop_server();

    // WiFi management
    bool connect_wifi(const char* ssid, const char* pass);
    void disconnect_wifi();
    WiFiState get_state() const { return wifi_state; }
    const char* get_ssid() const { return ssid.c_str(); }
    const char* get_ip() const { return ip_address.c_str(); }

    // WebSocket
    void broadcast_upload_progress(int percent, const char* filename);
    void broadcast_device_status(const char* status);
    void broadcast_file_list();

    // Settings
    bool save_settings(const char* ssid, const char* pass);
    bool load_settings(std::string& out_ssid, std::string& out_pass);
};

#endif // __NETWORK_MANAGER_H__
