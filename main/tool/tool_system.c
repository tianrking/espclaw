/*
 * ESPClaw - tool/tool_system.c
 * System diagnostics tool handler. Included by tool_registry.c.
 */
#include "hal/hal_gpio.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "platform.h"
#include <stdio.h>

static bool tool_system_diagnostics(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool tool_system_diagnostics(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    char gpio_pins[64] = {0};
    hal_gpio_allowed_pins_str(gpio_pins, sizeof(gpio_pins));

    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    snprintf(result_buf, result_sz,
             "Chip: %s | Heap: %lu bytes free | Uptime: %llus | GPIO allowed: [%s]",
             ESPCLAW_TARGET_NAME,
             (unsigned long)esp_get_free_heap_size(),
             uptime_s,
             gpio_pins[0] ? gpio_pins : "none");
    return true;
}
