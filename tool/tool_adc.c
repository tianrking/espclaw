/*
 * ESPClaw - tool/tool_adc.c
 * ADC tool handlers for analog sensors.
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
static bool tool_adc_set_atten(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_adc_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d not valid for ADC", pin);
        return false;
    }

    int raw, voltage_mv;
    esp_err_t err = hal_adc_read_raw(pin, &raw);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: ADC read failed (%s)", esp_err_to_name(err));
        return false;
    }

    err = hal_adc_read_voltage(pin, &voltage_mv);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: voltage conversion failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Calculate percentage (assuming 3.3V max) */
    int percent = (voltage_mv * 100) / 3300;

    snprintf(result_buf, result_sz, "ADC pin %d: raw=%d, voltage=%dmV (%d%%)", pin, raw, voltage_mv, percent);
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
        snprintf(result_buf, result_sz, "Error: pin %d not valid for ADC", pin);
        return false;
    }

    int voltage_mv;
    esp_err_t err = hal_adc_read_voltage(pin, &voltage_mv);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: voltage read failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Calculate percentage (assuming 3.3V max) */
    int percent = (voltage_mv * 100) / 3300;

    snprintf(result_buf, result_sz, "ADC pin %d: %dmV (%d%%)", pin, voltage_mv, percent);
    return true;
}

static bool tool_adc_read_avg(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, samples = 16;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "samples", &samples);
    if (samples < 1) samples = 1;
    if (samples > 64) samples = 64;

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d not valid for ADC", pin);
        return false;
    }

    int voltage_mv;
    esp_err_t err = hal_adc_read_averaged(pin, samples, &voltage_mv);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: ADC read failed (%s)", esp_err_to_name(err));
        return false;
    }

    /* Calculate percentage (assuming 3.3V max) */
    int percent = (voltage_mv * 100) / 3300;

    snprintf(result_buf, result_sz, "ADC pin %d: avg=%dmV (%d%%) over %d samples", pin, voltage_mv, percent, samples);
    return true;
}

static bool tool_adc_set_atten(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, atten = 3;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "atten", &atten);
    if (atten < 0 || atten > 3) {
        snprintf(result_buf, result_sz, "Error: atten must be 0-3 (0=1.1V, 1=1.5V, 2=2.2V, 3=3.3V)");
        return false;
    }

    if (!hal_adc_pin_valid(pin)) {
        snprintf(result_buf, result_sz, "Error: pin %d not valid for ADC", pin);
        return false;
    }

    esp_err_t err = hal_adc_set_atten(pin, atten);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to set attenuation (%s)", esp_err_to_name(err));
        return false;
    }

    const char *ranges[] = {"0-1.1V", "0-1.5V", "0-2.2V", "0-3.3V"};
    snprintf(result_buf, result_sz, "ADC pin %d: attenuation set to %s", pin, ranges[atten]);
    return true;
}
