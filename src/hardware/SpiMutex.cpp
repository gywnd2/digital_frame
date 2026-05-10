#include "../../include/hardware/SpiMutex.h"

SemaphoreHandle_t SpiMutex::spi_mutex = nullptr;
bool SpiMutex::initialized = false;

void SpiMutex::init() {
    if (!initialized) {
        spi_mutex = xSemaphoreCreateMutex();
        if (spi_mutex != nullptr) {
            initialized = true;
            Serial.println("[SPI Mutex] Initialized");
        } else {
            Serial.println("[SPI Mutex] ERROR: Failed to create mutex");
        }
    }
}

bool SpiMutex::lock(TickType_t timeout) {
    if (!initialized) return false;
    return xSemaphoreTake(spi_mutex, timeout) == pdTRUE;
}

void SpiMutex::unlock() {
    if (initialized) {
        xSemaphoreGive(spi_mutex);
    }
}
