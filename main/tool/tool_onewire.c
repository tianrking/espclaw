/*
 * ESPClaw - tool/tool_onewire.c
 * OneWire tool handlers for DS18B20 temperature sensors.
 * Included directly by tool_registry.c.
 */
#include "hal/hal_onewire.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

static bool tool_temp_scan(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_temp_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_temp_setup(const char *input_json, char *result_buf, size_t result_sz)
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
    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        snprintf(result_buf, result_sz, "Error: bus must be 0-%d", ONEWIRE_MAX_BUSES - 1);
        return false;
    }

    if (!hal_onewire_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d not allowed (range: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }

    esp_err_t err = hal_onewire_setup(pin, bus_id);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: OneWire setup failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "OneWire bus %d setup on pin %d. Requires 4.7K pull-up resistor.",
             bus_id, pin);
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
        snprintf(result_buf, result_sz, "Error: bus %d not configured. Call temp_setup first.", bus_id);
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
        snprintf(result_buf, result_sz, "No DS18B20 devices found on bus %d", bus_id);
        return true;
    }

    /* Build result string with device addresses */
    size_t pos = 0;
    pos += snprintf(result_buf + pos, result_sz - pos, "Found %d device(s) on bus %d:\n", found, bus_id);

    for (int i = 0; i < found && pos < result_sz - 20; i++) {
        char rom_str[20];
        hal_onewire_rom_to_str(&devices[i], rom_str, sizeof(rom_str));
        pos += snprintf(result_buf + pos, result_sz - pos,
                        "  [%d] %s (family: 0x%02X)\n",
                        i, rom_str, devices[i].family);
    }

    /* Remove trailing newline */
    if (result_buf[pos - 1] == '\n') result_buf[pos - 1] = '\0';

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
        snprintf(result_buf, result_sz, "Error: bus %d not configured. Call temp_setup first.", bus_id);
        return false;
    }

    /* Try to read from a specific device if ROM provided */
    char rom_str[32] = {0};
    onewire_rom_t *rom_ptr = NULL;
    onewire_rom_t specific_rom;

    if (json_get_str(input_json, "rom", rom_str, sizeof(rom_str)) && rom_str[0] != '\0') {
        /* Parse hex string to ROM (simplified - expects 16 hex chars) */
        if (strlen(rom_str) >= 16) {
            uint8_t bytes[8];
            for (int i = 0; i < 8; i++) {
                unsigned int byte;
                sscanf(rom_str + i * 2, "%2x", &byte);
                bytes[i] = (uint8_t)byte;
            }
            specific_rom.family = bytes[0];
            memcpy(specific_rom.serial, bytes + 1, 6);
            specific_rom.crc = bytes[7];
            rom_ptr = &specific_rom;
        }
    }

    onewire_temp_t result;
    esp_err_t err = hal_onewire_read_temp(bus_id, rom_ptr, &result);

    if (err == ESP_ERR_NOT_FOUND) {
        snprintf(result_buf, result_sz, "Error: no device found on bus %d", bus_id);
        return false;
    }
    if (err == ESP_ERR_INVALID_CRC) {
        snprintf(result_buf, result_sz, "Error: CRC check failed (communication error)");
        return false;
    }
    if (err == ESP_ERR_TIMEOUT) {
        snprintf(result_buf, result_sz, "Error: device did not respond (timeout)");
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: temperature read failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Format temperature (temp_c_x100 is °C * 100) */
    float temp_c = result.temp_c_x100 / 100.0f;
    char device_rom[20];
    hal_onewire_rom_to_str(&result.rom, device_rom, sizeof(device_rom));

    snprintf(result_buf, result_sz, "Temperature: %.2f °C (device: %s)",
             temp_c, device_rom);
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
