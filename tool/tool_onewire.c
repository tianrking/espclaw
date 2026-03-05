/*
 * ESPClaw - tool/tool_onewire.c
 * OneWire tool handlers for DS18B20 temperature sensors.
 * Included directly by tool_registry.c.
 */
#include "hal/hal_onewire.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

/* Forward declarations - using temp_* naming for tool registration */
static bool tool_temp_setup(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_temp_scan(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_temp_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_temp_release(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_temp_setup(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, bus_id = 0;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "bus", &bus_id);
    if (bus_id < 0) bus_id = 0;
    if (bus_id >= ONEWIRE_MAX_BUSES) {
        snprintf(result_buf, result_sz, "Error: bus must be 0-%d", ONEWIRE_MAX_BUSES - 1);
        return false;
    }

    esp_err_t err = hal_onewire_setup(pin, bus_id);
    if (err == ESP_ERR_INVALID_ARG) {
        snprintf(result_buf, result_sz, "Error: pin %d not allowed (range: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: OneWire setup failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "OneWire bus %d: pin %d ready (requires 4.7K pull-up)", bus_id, pin);
    return true;
}

static bool tool_temp_scan(const char *input_json, char *result_buf, size_t result_sz)
{
    int bus_id = 0;
    json_get_int(input_json, "bus", &bus_id);

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        snprintf(result_buf, result_sz, "Error: bus must be 0-%d", ONEWIRE_MAX_BUSES - 1);
        return false;
    }

    if (!hal_onewire_bus_active(bus_id)) {
        snprintf(result_buf, result_sz, "Error: bus %d not configured. Call onewire_setup first.", bus_id);
        return false;
    }

    onewire_rom_t devices[ONEWIRE_MAX_DEVICES];
    int found = 0;

    esp_err_t err = hal_onewire_scan(bus_id, devices, ONEWIRE_MAX_DEVICES, &found);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: scan failed (%s)", esp_err_to_name(err));
        return false;
    }

    if (found == 0) {
        snprintf(result_buf, result_sz, "No devices found on bus %d", bus_id);
        return true;
    }

    /* Build result string with device addresses */
    int offset = snprintf(result_buf, result_sz, "Bus %d: %d device(s) found", bus_id, found);
    for (int i = 0; i < found && offset < (int)result_sz - 20; i++) {
        char rom_str[20];
        hal_onewire_rom_to_str(&devices[i], rom_str, sizeof(rom_str));
        offset += snprintf(result_buf + offset, result_sz - offset, "\n  [%d] %s", i, rom_str);
    }

    return true;
}

static bool tool_temp_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int bus_id = 0;
    json_get_int(input_json, "bus", &bus_id);

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        snprintf(result_buf, result_sz, "Error: bus must be 0-%d", ONEWIRE_MAX_BUSES - 1);
        return false;
    }

    if (!hal_onewire_bus_active(bus_id)) {
        snprintf(result_buf, result_sz, "Error: bus %d not configured. Call onewire_setup first.", bus_id);
        return false;
    }

    /* Check if reading all or specific device */
    char rom_str[20] = {0};
    const onewire_rom_t *rom_ptr = NULL;
    onewire_rom_t specific_rom;

    if (json_get_string(input_json, "rom", rom_str, sizeof(rom_str))) {
        /* Parse hex address to ROM */
        if (strlen(rom_str) != 16) {
            snprintf(result_buf, result_sz, "Error: address must be 16 hex chars");
            return false;
        }
        uint8_t *rom_bytes = (uint8_t*)&specific_rom;
        for (int i = 0; i < 8; i++) {
            unsigned int byte;
            if (sscanf(rom_str + i*2, "%2x", &byte) != 1) {
                snprintf(result_buf, result_sz, "Error: invalid hex address");
                return false;
            }
            rom_bytes[i] = (uint8_t)byte;
        }
        rom_ptr = &specific_rom;
    }

    onewire_temp_t result;
    esp_err_t err = hal_onewire_read_temp(bus_id, rom_ptr, &result);
    if (err == ESP_ERR_NOT_FOUND) {
        snprintf(result_buf, result_sz, "Error: no device found on bus %d", bus_id);
        return false;
    }
    if (err == ESP_ERR_INVALID_CRC) {
        snprintf(result_buf, result_sz, "Error: CRC check failed, retry reading");
        return false;
    }
    if (err == ESP_ERR_TIMEOUT) {
        snprintf(result_buf, result_sz, "Error: device timeout");
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: read failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Convert temp_c_x100 to human readable */
    int temp_whole = result.temp_c_x100 / 100;
    int temp_frac = abs(result.temp_c_x100 % 100);

    if (rom_ptr) {
        hal_onewire_rom_to_str(&result.rom, rom_str, sizeof(rom_str));
        snprintf(result_buf, result_sz, "Temperature: %d.%02d°C (device: %s)", temp_whole, temp_frac, rom_str);
    } else {
        snprintf(result_buf, result_sz, "Temperature: %d.%02d°C", temp_whole, temp_frac);
    }

    return true;
}

static bool tool_temp_release(const char *input_json, char *result_buf, size_t result_sz)
{
    int bus_id = 0;
    json_get_int(input_json, "bus", &bus_id);

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        snprintf(result_buf, result_sz, "Error: bus must be 0-%d", ONEWIRE_MAX_BUSES - 1);
        return false;
    }

    esp_err_t err = hal_onewire_release(bus_id);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: release failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "OneWire bus %d released", bus_id);
    return true;
}
