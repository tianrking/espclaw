/*
 * ESPClaw - tool/tool_gpio.c
 * GPIO tool handlers. Included directly by tool_registry.c (not a standalone TU).
 */
#include "hal/hal_gpio.h"
#include "util/json_util.h"
#include "config.h"
#include <stdio.h>

static bool tool_gpio_write(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_gpio_read(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_gpio_read_all(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_gpio_write(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1, state = -1;
    if (!json_get_int(input_json, "pin",   &pin)   || pin   < 0 ||
        !json_get_int(input_json, "state", &state)) {
        snprintf(result_buf, result_sz, "Error: missing pin or state");
        return false;
    }
    esp_err_t err = hal_gpio_write(pin, state);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz,
                 "Error: GPIO %d not allowed (allowed: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    snprintf(result_buf, result_sz, "GPIO %d set to %s", pin, state ? "HIGH" : "LOW");
    return true;
}

static bool tool_gpio_read(const char *input_json, char *result_buf, size_t result_sz)
{
    int pin = -1;
    if (!json_get_int(input_json, "pin", &pin) || pin < 0) {
        snprintf(result_buf, result_sz, "Error: missing pin");
        return false;
    }
    int level = 0;
    esp_err_t err = hal_gpio_read(pin, &level);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz,
                 "Error: GPIO %d not allowed (allowed: %d-%d)",
                 pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    snprintf(result_buf, result_sz, "GPIO %d: %s", pin, level ? "HIGH" : "LOW");
    return true;
}

static bool tool_gpio_read_all(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    size_t pos = 0;
    bool first = true;
    for (int p = GPIO_MIN_PIN; p <= GPIO_MAX_PIN; p++) {
        if (!hal_gpio_is_allowed(p)) continue;
        int level = 0;
        hal_gpio_read(p, &level);
        int n = snprintf(result_buf + pos, result_sz - pos,
                         "%sGPIO %d: %s", first ? "" : ", ", p, level ? "HIGH" : "LOW");
        if (n <= 0 || (size_t)n >= result_sz - pos) break;
        pos += n;
        first = false;
    }
    if (first) snprintf(result_buf, result_sz, "No allowed GPIO pins");
    return true;
}
