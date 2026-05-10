#ifndef __SPI_MUTEX_H__
#define __SPI_MUTEX_H__

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SpiMutex {
private:
    static SemaphoreHandle_t spi_mutex;
    static bool initialized;

public:
    static void init();
    static bool lock(TickType_t timeout = portMAX_DELAY);
    static void unlock();

    // RAII wrapper for automatic unlock
    class Guard {
    private:
        bool locked;
    public:
        Guard() : locked(false) {
            locked = SpiMutex::lock();
        }
        ~Guard() {
            if (locked) SpiMutex::unlock();
        }
        bool is_locked() const { return locked; }
    };
};

#endif // __SPI_MUTEX_H__
