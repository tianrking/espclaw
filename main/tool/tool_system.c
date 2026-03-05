/*
 * ESPClaw - tool/tool_system.c
 * System diagnostics and utility tool handlers. Included by tool_registry.c.
 */
#include "hal/hal_gpio.h"
#include "service/cron_service.h"
#include "util/ratelimit.h"
#include "util/json_util.h"
#include "config.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

/* Forward declarations */
static bool tool_system_diagnostics(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_delay(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_get_version(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

/* -----------------------------------------------------------------------
 * Tool: delay
 * Wait for specified milliseconds (max 60000). Use between GPIO operations.
 * ----------------------------------------------------------------------- */
static bool tool_delay(const char *input_json, char *result_buf, size_t result_sz)
{
    int ms = 0;
    if (!json_get_int(input_json, "milliseconds", &ms) || ms <= 0) {
        snprintf(result_buf, result_sz, "Error: 'milliseconds' required (positive integer)");
        return false;
    }
    if (ms > 60000) {
        ms = 60000;  /* Cap at 60 seconds */
    }

    vTaskDelay(pdMS_TO_TICKS(ms));
    snprintf(result_buf, result_sz, "Waited %d ms", ms);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: get_version
 * Get firmware version and build info.
 * ----------------------------------------------------------------------- */
static bool tool_get_version(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    snprintf(result_buf, result_sz,
        "ESPClaw v%s | Target: %s | ESP-IDF: %s",
        ESPCLAW_VERSION,
        ESPCLAW_TARGET_NAME,
        IDF_VER);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: get_diagnostics (enhanced)
 * Get device health: heap, uptime, rate limits, time sync, version.
 * ----------------------------------------------------------------------- */
static bool tool_system_diagnostics(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;

    /* GPIO allowed pins */
    char gpio_pins[64] = {0};
    hal_gpio_allowed_pins_str(gpio_pins, sizeof(gpio_pins));

    /* Uptime */
    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    char uptime_str[32] = {0};
    if (uptime_s >= 86400) {
        snprintf(uptime_str, sizeof(uptime_str), "%llud %02lluh %02llum %02llus",
            uptime_s / 86400, (uptime_s % 86400) / 3600,
            (uptime_s % 3600) / 60, uptime_s % 60);
    } else if (uptime_s >= 3600) {
        snprintf(uptime_str, sizeof(uptime_str), "%lluh %02llum %02llus",
            uptime_s / 3600, (uptime_s % 3600) / 60, uptime_s % 60);
    } else if (uptime_s >= 60) {
        snprintf(uptime_str, sizeof(uptime_str), "%llum %02llus",
            uptime_s / 60, uptime_s % 60);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%llus", uptime_s);
    }

    /* Memory info */
    uint32_t free_heap = (uint32_t)esp_get_free_heap_size();
    uint32_t min_heap = (uint32_t)esp_get_minimum_free_heap_size();
    uint32_t largest_block = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    /* Rate limit info */
    int req_hour = ratelimit_get_requests_this_hour();
    int req_day = ratelimit_get_requests_today();

    /* Time sync info */
    bool synced = cron_is_time_synced();
    char tz_str[TIMEZONE_MAX_LEN] = {0};
    cron_get_timezone(tz_str, sizeof(tz_str));

    snprintf(result_buf, result_sz,
        "Diag: uptime=%s | heap=%lu/%lu/%lu | req=%d/hr,%d/day | time=%s | tz=%s | gpio=[%s] | v=%s",
        uptime_str,
        (unsigned long)free_heap,
        (unsigned long)min_heap,
        (unsigned long)largest_block,
        req_hour, req_day,
        synced ? "synced" : "not synced",
        tz_str,
        gpio_pins[0] ? gpio_pins : "none",
        ESPCLAW_VERSION);

    return true;
}
