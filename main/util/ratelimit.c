/*
 * ESPClaw - util/ratelimit.c
 * LLM API rate limiting implementation.
 */
#include "util/ratelimit.h"
#include "config.h"
#include "nvs_keys.h"
#include "mem/nvs_manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>

static const char *TAG = "ratelimit";

/* Sliding window counters */
static int s_requests_this_hour = 0;
static int s_requests_today = 0;
static int s_last_hour = -1;
static int s_last_day = -1;
static int s_last_year = -1;

/* Track persistence failures */
static int s_persist_failure_count = 0;

/* -----------------------------------------------------------------------
 * Helper: Persist value to NVS
 * ----------------------------------------------------------------------- */
static void persist_value(const char *key, const char *value)
{
    esp_err_t err = nvs_mgr_set_str(key, value);
    if (err != ESP_OK) {
        s_persist_failure_count++;
        ESP_LOGW(TAG, "Failed to persist %s: %s", key, esp_err_to_name(err));
    }
}

/* -----------------------------------------------------------------------
 * Helper: Parse integer from string with fallback
 * ----------------------------------------------------------------------- */
static int parse_int_or_default(const char *value, int fallback)
{
    if (!value || value[0] == '\0') {
        return fallback;
    }

    char *endptr = NULL;
    long parsed = strtol(value, &endptr, 10);

    if (!endptr || endptr == value || *endptr != '\0') {
        return fallback;
    }

    if (parsed < INT_MIN || parsed > INT_MAX) {
        return fallback;
    }

    return (int)parsed;
}

/* -----------------------------------------------------------------------
 * Initialize rate limiter from NVS
 * ----------------------------------------------------------------------- */
void ratelimit_init(void)
{
    s_persist_failure_count = 0;
    s_requests_this_hour = 0;

    /* Load persisted daily count */
    char buf[16];
    if (nvs_mgr_get_str(NVS_KEY_RL_DAILY, buf, sizeof(buf)) == ESP_OK) {
        s_requests_today = parse_int_or_default(buf, 0);
    }
    if (nvs_mgr_get_str(NVS_KEY_RL_DAY, buf, sizeof(buf)) == ESP_OK) {
        s_last_day = parse_int_or_default(buf, -1);
    }
    if (nvs_mgr_get_str(NVS_KEY_RL_YEAR, buf, sizeof(buf)) == ESP_OK) {
        s_last_year = parse_int_or_default(buf, -1);
    }

    ESP_LOGI(TAG, "Rate limiter initialized: %d requests today (limit: %d/hour, %d/day)",
             s_requests_today, RATELIMIT_MAX_PER_HOUR, RATELIMIT_MAX_PER_DAY);
}

/* -----------------------------------------------------------------------
 * Update time window and reset counters as needed
 * ----------------------------------------------------------------------- */
static void update_time_window(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int current_hour = timeinfo.tm_hour;
    int current_day = timeinfo.tm_yday;
    int current_year = timeinfo.tm_year;

    /* Reset hourly counter when hour changes */
    if (current_hour != s_last_hour) {
        s_requests_this_hour = 0;
        s_last_hour = current_hour;
        ESP_LOGD(TAG, "Hourly counter reset (hour %d)", current_hour);
    }

    /* Reset daily counter when day or year changes */
    if (current_day != s_last_day || current_year != s_last_year) {
        s_requests_today = 0;
        s_last_day = current_day;
        s_last_year = current_year;

        /* Persist the new day/year markers */
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", current_day);
        persist_value(NVS_KEY_RL_DAY, buf);
        snprintf(buf, sizeof(buf), "%d", current_year);
        persist_value(NVS_KEY_RL_YEAR, buf);
        persist_value(NVS_KEY_RL_DAILY, "0");

        ESP_LOGI(TAG, "Daily rate limit reset (day %d, year %d)", current_day, current_year);
    }
}

/* -----------------------------------------------------------------------
 * Check if request is allowed
 * ----------------------------------------------------------------------- */
bool ratelimit_check(char *reason, size_t reason_len)
{
#if !RATELIMIT_ENABLED
    (void)reason;
    (void)reason_len;
    return true;
#endif

    update_time_window();

    /* Check hourly limit */
    if (s_requests_this_hour >= RATELIMIT_MAX_PER_HOUR) {
        snprintf(reason, reason_len,
                 "Rate limited: %d/%d requests this hour. Please try again later.",
                 s_requests_this_hour, RATELIMIT_MAX_PER_HOUR);
        ESP_LOGW(TAG, "Hourly rate limit exceeded (%d/%d)",
                 s_requests_this_hour, RATELIMIT_MAX_PER_HOUR);
        return false;
    }

    /* Check daily limit */
    if (s_requests_today >= RATELIMIT_MAX_PER_DAY) {
        snprintf(reason, reason_len,
                 "Daily limit reached: %d/%d requests today. Resets at midnight.",
                 s_requests_today, RATELIMIT_MAX_PER_DAY);
        ESP_LOGW(TAG, "Daily rate limit exceeded (%d/%d)",
                 s_requests_today, RATELIMIT_MAX_PER_DAY);
        return false;
    }

    return true;
}

/* -----------------------------------------------------------------------
 * Record that a request was made
 * ----------------------------------------------------------------------- */
void ratelimit_record_request(void)
{
    update_time_window();

    s_requests_this_hour++;
    s_requests_today++;

    /* Persist daily count */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s_requests_today);
    persist_value(NVS_KEY_RL_DAILY, buf);

    ESP_LOGD(TAG, "Request recorded: %d/hour, %d/day",
             s_requests_this_hour, s_requests_today);
}

/* -----------------------------------------------------------------------
 * Get current statistics
 * ----------------------------------------------------------------------- */
int ratelimit_get_requests_today(void)
{
    return s_requests_today;
}

int ratelimit_get_requests_this_hour(void)
{
    return s_requests_this_hour;
}

/* -----------------------------------------------------------------------
 * Manual reset (for testing or admin)
 * ----------------------------------------------------------------------- */
void ratelimit_reset_daily(void)
{
    s_requests_today = 0;
    s_requests_this_hour = 0;
    persist_value(NVS_KEY_RL_DAILY, "0");
    ESP_LOGI(TAG, "Rate limits manually reset");
}
