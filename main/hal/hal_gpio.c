/*
 * ESPClaw - hal/hal_gpio.c
 * GPIO HAL implementation with Kconfig-driven safety guardrails.
 */
#include "hal_gpio.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "hal_gpio";

esp_err_t hal_gpio_init(void)
{
    ESP_LOGI(TAG, "GPIO HAL ready: pins %d-%d", GPIO_MIN_PIN, GPIO_MAX_PIN);
    return ESP_OK;
}

bool hal_gpio_is_allowed(int pin)
{
    if (pin < GPIO_MIN_PIN || pin > GPIO_MAX_PIN) return false;

    /* Optional fine-grained CSV allowlist */
    const char *csv = GPIO_ALLOWED_PINS_CSV;
    if (!csv || csv[0] == '\0') return true;  /* no CSV = all in range OK */

    char tmp[64];
    strncpy(tmp, csv, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *tok = strtok(tmp, ",");
    while (tok) {
        if (atoi(tok) == pin) return true;
        tok = strtok(NULL, ",");
    }
    return false;
}

esp_err_t hal_gpio_write(int pin, int level)
{
    if (!hal_gpio_is_allowed(pin)) {
        ESP_LOGW(TAG, "GPIO %d not allowed", pin);
        return ESP_ERR_INVALID_ARG;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(pin, level ? 1 : 0);
    ESP_LOGI(TAG, "GPIO %d → %s", pin, level ? "HIGH" : "LOW");
    return ESP_OK;
}

esp_err_t hal_gpio_read(int pin, int *level)
{
    if (!hal_gpio_is_allowed(pin)) {
        ESP_LOGW(TAG, "GPIO %d not allowed", pin);
        return ESP_ERR_INVALID_ARG;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    *level = gpio_get_level(pin);
    return ESP_OK;
}

void hal_gpio_allowed_pins_str(char *buf, size_t buf_sz)
{
    size_t pos = 0;
    bool first = true;
    for (int p = GPIO_MIN_PIN; p <= GPIO_MAX_PIN; p++) {
        if (!hal_gpio_is_allowed(p)) continue;
        int n = snprintf(buf + pos, buf_sz - pos, "%s%d", first ? "" : ",", p);
        if (n <= 0 || (size_t)n >= buf_sz - pos) break;
        pos += n;
        first = false;
    }
    buf[pos] = '\0';
}
