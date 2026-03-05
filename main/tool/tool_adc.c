/*
 * ESPClaw - tool/tool_adc.c
 * ADC tool handlers for analog sensor reading.
 * Included directly by tool_registry.c.
 */
#include "hal/hal_adc.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

static bool tool_adc_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_adc_read_voltage(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_adc_read_avg(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_adc_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d does not support ADC", pin);
        return false;
    }

    int raw_value;
    esp_err_t err = hal_adc_read_raw(pin, &raw_value);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: ADC read failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "ADC pin %d: raw value %d (0-4095)", pin, raw_value);
    return true;
}

static bool tool_adc_read_voltage(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d does not support ADC", pin);
        return false;
    }

    int voltage_mv;
    esp_err_t err = hal_adc_read_voltage(pin, &voltage_mv);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: ADC voltage read failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Format voltage nicely */
    float voltage_v = voltage_mv / 1000.0f;
    snprintf(result_buf, result_sz, "ADC pin %d: %.3f V (%d mV)", pin, voltage_v, voltage_mv);
    return true;
}

static bool tool_adc_read_avg(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, samples = 16;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d does not support ADC", pin);
        return false;
    }

    json_get_int(input_json, "samples", &samples);
    if (samples < 1) samples = 1;
    if (samples > 64) samples = 64;

    int voltage_mv;
    esp_err_t err = hal_adc_read_averaged(pin, samples, &voltage_mv);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: ADC averaged read failed (%s)", esp_err_to_name(err));
        return false;
    }

    float voltage_v = voltage_mv / 1000.0f;
    snprintf(result_buf, result_sz, "ADC pin %d: %.3f V (%d mV, avg of %d samples)",
             pin, voltage_v, voltage_mv, samples);
    return true;
}
