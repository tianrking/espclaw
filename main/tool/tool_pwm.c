/*
 * ESPClaw - tool/tool_pwm.c
 * PWM tool handlers for LED dimming, motor speed control.
 * Included directly by tool_registry.c.
 */
#include "hal/hal_pwm.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

static bool tool_pwm_setup(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_pwm_set_duty(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_pwm_set_freq(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_pwm_stop(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_pwm_status(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_pwm_setup(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, channel = -1, freq = 5000;

    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'pin'");
        return false;
    }

    json_get_int(input_json, "channel", &channel);
    json_get_int(input_json, "freq", &freq);

    /* Default channel to 0 if not specified */
    if (channel < 0) channel = 0;
    if (channel >= PWM_MAX_CHANNELS) {
        snprintf(result_buf, result_sz, "Error: channel must be 0-%d", PWM_MAX_CHANNELS - 1);
        return false;
    }

    /* Validate frequency */
    if (freq < PWM_MIN_FREQ_HZ || freq > PWM_MAX_FREQ_HZ) {
        snprintf(result_buf, result_sz, "Error: freq must be %d-%d Hz", PWM_MIN_FREQ_HZ, PWM_MAX_FREQ_HZ);
        return false;
    }

    esp_err_t err = hal_pwm_setup(pin, channel, (uint32_t)freq);
    if (err == ESP_ERR_INVALID_ARG) {
        snprintf(result_buf, result_sz, "Error: pin %d not allowed (range: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: PWM setup failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "PWM ch%d: pin %d at %d Hz ready", channel, pin, freq);
    return true;
}

static bool tool_pwm_set_duty(const char *input_json, char *result_buf, size_t result_sz)
{
    int channel = -1, duty = -1;

    if (!json_get_int(input_json, "channel", &channel) || channel < 0) {
        snprintf(result_buf, result_sz, "Error: missing 'channel'");
        return false;
    }

    if (!json_get_int(input_json, "duty", &duty) || duty < 0) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'duty'");
        return false;
    }

    if (channel >= PWM_MAX_CHANNELS) {
        snprintf(result_buf, result_sz, "Error: channel must be 0-%d", PWM_MAX_CHANNELS - 1);
        return false;
    }

    /* Clamp duty to 0-100 */
    if (duty > 100) duty = 100;

    esp_err_t err = hal_pwm_set_duty(channel, (uint32_t)duty);
    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: channel %d not configured. Call pwm_setup first.", channel);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to set duty (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "PWM ch%d: duty %d%%", channel, duty);
    return true;
}

static bool tool_pwm_set_freq(const char *input_json, char *result_buf, size_t result_sz)
{
    int channel = -1, freq = -1;

    if (!json_get_int(input_json, "channel", &channel) || channel < 0) {
        snprintf(result_buf, result_sz, "Error: missing 'channel'");
        return false;
    }

    if (!json_get_int(input_json, "freq", &freq) || freq < PWM_MIN_FREQ_HZ) {
        snprintf(result_buf, result_sz, "Error: missing or invalid 'freq'");
        return false;
    }

    if (channel >= PWM_MAX_CHANNELS) {
        snprintf(result_buf, result_sz, "Error: channel must be 0-%d", PWM_MAX_CHANNELS - 1);
        return false;
    }

    if (freq > PWM_MAX_FREQ_HZ) {
        snprintf(result_buf, result_sz, "Error: freq must be <= %d Hz", PWM_MAX_FREQ_HZ);
        return false;
    }

    esp_err_t err = hal_pwm_set_freq(channel, (uint32_t)freq);
    if (err == ESP_ERR_INVALID_STATE) {
        snprintf(result_buf, result_sz, "Error: channel %d not configured", channel);
        return false;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to set frequency (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "PWM ch%d: frequency set to %d Hz", channel, freq);
    return true;
}

static bool tool_pwm_stop(const char *input_json, char *result_buf, size_t result_sz)
{
    int channel = -1;

    if (!json_get_int(input_json, "channel", &channel) || channel < 0) {
        snprintf(result_buf, result_sz, "Error: missing 'channel'");
        return false;
    }

    if (channel >= PWM_MAX_CHANNELS) {
        snprintf(result_buf, result_sz, "Error: channel must be 0-%d", PWM_MAX_CHANNELS - 1);
        return false;
    }

    esp_err_t err = hal_pwm_stop(channel);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to stop PWM (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "PWM ch%d stopped", channel);
    return true;
}

static bool tool_pwm_status(const char *input_json, char *result_buf, size_t result_sz)
{
    int channel = -1;

    if (!json_get_int(input_json, "channel", &channel) || channel < 0) {
        snprintf(result_buf, result_sz, "Error: missing 'channel'");
        return false;
    }

    if (channel >= PWM_MAX_CHANNELS) {
        snprintf(result_buf, result_sz, "Error: channel must be 0-%d", PWM_MAX_CHANNELS - 1);
        return false;
    }

    uint32_t freq, duty;
    esp_err_t err = hal_pwm_get_status(channel, &freq, &duty);
    if (err == ESP_ERR_NOT_FOUND) {
        snprintf(result_buf, result_sz, "PWM ch%d: not configured", channel);
        return true;
    }
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to get status (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result_buf, result_sz, "PWM ch%d: %lu Hz, %lu%% duty", channel, freq, duty);
    return true;
}
