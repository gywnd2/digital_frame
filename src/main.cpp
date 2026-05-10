#include <Arduino.h>
#include <app/Application.h>

Application app;

static void handle_serial_command() {
    if (Serial.available() <= 0) {
        return;
    }

    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();

    if (input == "reset") {
        Serial.println("[System] Reset command received. Rebooting...");
        delay(500);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    delay(1000);

    app.init();
}

void loop() {
    handle_serial_command();
    app.run();
    vTaskDelay(pdMS_TO_TICKS(100));
}
