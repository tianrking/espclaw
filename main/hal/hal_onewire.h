/*
 * ESPClaw - hal/hal_onewire.h
 * OneWire HAL interface for DS18B20 temperature sensors.
 * Bit-banged implementation optimized for reliability.
 */
#ifndef HAL_ONEWIRE_H
#define HAL_ONEWIRE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* OneWire configuration */
#define ONEWIRE_MAX_DEVICES     4       /* Maximum devices on bus */
#define ONEWIRE_MAX_BUSES       2       /* Maximum separate buses */
#define ONEWIRE_ROM_SIZE        8       /* 64-bit ROM code */
#define ONEWIRE_TIMEOUT_MS      1000    /* Operation timeout */

/* Temperature reading precision (9-12 bits) */
#define ONEWIRE_RES_9BIT        9       /* 0.5°C, 94ms conversion */
#define ONEWIRE_RES_10BIT       10      /* 0.25°C, 188ms conversion */
#define ONEWIRE_RES_11BIT       11      /* 0.125°C, 375ms conversion */
#define ONEWIRE_RES_12BIT       12      /* 0.0625°C, 750ms conversion */

/* Device ROM code structure */
typedef struct {
    uint8_t family;     /* Family code (0x28 = DS18B20) */
    uint8_t serial[6];  /* Unique serial number */
    uint8_t crc;        /* CRC8 checksum */
} onewire_rom_t;

/* Temperature reading result */
typedef struct {
    onewire_rom_t rom;      /* Device ROM code */
    int16_t temp_c_x100;    /* Temperature in °C * 100 (e.g., 2350 = 23.50°C) */
    bool valid;             /* CRC check passed */
} onewire_temp_t;

/**
 * Initialize OneWire HAL.
 * Call once at startup.
 */
esp_err_t hal_onewire_init(void);

/**
 * Deinitialize OneWire HAL and release all resources.
 */
esp_err_t hal_onewire_deinit(void);

/**
 * Check if a GPIO pin is suitable for OneWire.
 * Requires bi-directional capability.
 */
bool hal_onewire_pin_valid(int pin);

/**
 * Setup a OneWire bus on a GPIO pin.
 * @param pin   GPIO pin number (requires 4.7K pull-up resistor)
 * @param bus_id Bus ID (0-1) for tracking multiple buses
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_setup(int pin, int bus_id);

/**
 * Release a OneWire bus.
 * @param bus_id Bus ID (0-1)
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_release(int bus_id);

/**
 * Scan for devices on a bus.
 * @param bus_id     Bus ID (0-1)
 * @param devices    Output array for found ROM codes
 * @param max_count  Maximum devices to find
 * @param found      Output: number of devices found
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_scan(int bus_id, onewire_rom_t *devices,
                           int max_count, int *found);

/**
 * Read temperature from a specific device.
 * @param bus_id  Bus ID (0-1)
 * @param rom     Device ROM code (NULL for first/only device)
 * @param result  Output: temperature reading
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_read_temp(int bus_id, const onewire_rom_t *rom,
                                onewire_temp_t *result);

/**
 * Read temperature from all devices on a bus.
 * @param bus_id   Bus ID (0-1)
 * @param results  Output array for temperature readings
 * @param max_count Maximum results to store
 * @param found    Output: number of devices read
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_read_all(int bus_id, onewire_temp_t *results,
                               int max_count, int *found);

/**
 * Set temperature conversion resolution (9-12 bits).
 * Higher resolution = longer conversion time.
 * @param bus_id Bus ID (0-1)
 * @param rom    Device ROM code (NULL for all devices)
 * @param bits   Resolution bits (9-12)
 * @return ESP_OK on success
 */
esp_err_t hal_onewire_set_resolution(int bus_id, const onewire_rom_t *rom,
                                     int bits);

/**
 * Check if a bus is active.
 */
bool hal_onewire_bus_active(int bus_id);

/**
 * Get the pin number for an active bus.
 * @param bus_id Bus ID (0-1)
 * @return Pin number, or -1 if not active
 */
int hal_onewire_get_pin(int bus_id);

/**
 * Convert ROM code to hex string.
 * @param rom    Device ROM code
 * @param buf    Output buffer (at least 17 chars)
 * @param buf_sz Buffer size
 */
void hal_onewire_rom_to_str(const onewire_rom_t *rom, char *buf, size_t buf_sz);

#endif /* HAL_ONEWIRE_H */
