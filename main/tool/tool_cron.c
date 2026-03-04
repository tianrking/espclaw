/*
 * ESPClaw - tool/tool_cron.c
 * Cron scheduled task tools. Included directly by tool_registry.c.
 */
#include "tool/tool.h"
#include "service/cron_service.h"
#include "util/json_util.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations */
static bool tool_cron_schedule(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_cron_list(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_cron_cancel(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_cron_cancel_all(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_get_time(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_set_timezone(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

/* -----------------------------------------------------------------------
 * Timezone aliases for common names
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *alias;
    const char *posix;
} tz_alias_t;

static const tz_alias_t TZ_ALIASES[] = {
    {"UTC", "UTC0"},
    {"GMT", "UTC0"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Beijing", "CST-8"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Seoul", "KST-9"},
    {"Asia/Singapore", "SGT-8"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"America/Chicago", "CST6CDT,M3.2.0/2,M11.1.0/2"},
    {"America/Denver", "MST7MDT,M3.2.0/2,M11.1.0/2"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};

static const char *resolve_timezone(const char *tz_name)
{
    /* Check if it's already a POSIX string (contains digit or sign) */
    for (const char *p = tz_name; *p; p++) {
        if ((*p >= '0' && *p <= '9') || *p == '+' || *p == '-') {
            return tz_name;  /* Assume it's already POSIX format */
        }
    }

    /* Look up alias */
    for (size_t i = 0; i < sizeof(TZ_ALIASES) / sizeof(TZ_ALIASES[0]); i++) {
        if (strcasecmp(tz_name, TZ_ALIASES[i].alias) == 0) {
            return TZ_ALIASES[i].posix;
        }
    }

    return NULL;  /* Not found */
}

/* -----------------------------------------------------------------------
 * Tool: cron_schedule
 * ----------------------------------------------------------------------- */
static bool tool_cron_schedule(const char *input_json, char *result_buf, size_t result_sz)
{
    char type_str[16] = {0};
    char action[CRON_MAX_ACTION_LEN] = {0};
    int interval_seconds = 0;
    int minute = 0;

    if (!json_get_str(input_json, "type", type_str, sizeof(type_str)) ||
        strlen(type_str) == 0) {
        snprintf(result_buf, result_sz, "Error: 'type' required (periodic/daily/once)");
        return false;
    }

    if (!json_get_str(input_json, "action", action, sizeof(action)) ||
        strlen(action) == 0) {
        snprintf(result_buf, result_sz, "Error: 'action' required (what to do)");
        return false;
    }

    cron_type_t type;
    if (strcmp(type_str, "periodic") == 0) {
        type = CRON_TYPE_PERIODIC;

        /* Try interval_seconds first, then fall back to interval_minutes */
        int seconds_val = 0, minutes_val = 0;
        bool has_seconds = json_get_int(input_json, "interval_seconds", &seconds_val);
        bool has_minutes = json_get_int(input_json, "interval_minutes", &minutes_val);

        if (has_seconds && seconds_val > 0) {
            interval_seconds = seconds_val;
        } else if (has_minutes && minutes_val > 0) {
            interval_seconds = minutes_val * 60;  /* Convert to seconds */
        } else {
            snprintf(result_buf, result_sz, "Error: 'interval_seconds' or 'interval_minutes' required for periodic (min 10 seconds)");
            return false;
        }

        /* Enforce minimum interval */
        if (interval_seconds < CRON_MIN_INTERVAL_SECONDS) {
            interval_seconds = CRON_MIN_INTERVAL_SECONDS;
        }
    } else if (strcmp(type_str, "daily") == 0) {
        type = CRON_TYPE_DAILY;
        int hour = 0;
        if (!json_get_int(input_json, "hour", &hour) ||
            hour < 0 || hour > 23) {
            snprintf(result_buf, result_sz, "Error: 'hour' required for daily (0-23)");
            return false;
        }
        interval_seconds = hour;  /* Reuse for hour */
        if (!json_get_int(input_json, "minute", &minute)) {
            minute = 0;
        }
        if (minute < 0 || minute > 59) {
            snprintf(result_buf, result_sz, "Error: 'minute' must be 0-59");
            return false;
        }
    } else if (strcmp(type_str, "once") == 0) {
        type = CRON_TYPE_ONCE;

        /* Try delay_seconds first, then fall back to delay_minutes */
        int seconds_val = 0, minutes_val = 0;
        bool has_seconds = json_get_int(input_json, "delay_seconds", &seconds_val);
        bool has_minutes = json_get_int(input_json, "delay_minutes", &minutes_val);

        if (has_seconds && seconds_val > 0) {
            interval_seconds = seconds_val;
        } else if (has_minutes && minutes_val > 0) {
            interval_seconds = minutes_val * 60;  /* Convert to seconds */
        } else {
            snprintf(result_buf, result_sz, "Error: 'delay_seconds' or 'delay_minutes' required for once (min 10 seconds)");
            return false;
        }

        /* Enforce minimum interval */
        if (interval_seconds < CRON_MIN_INTERVAL_SECONDS) {
            interval_seconds = CRON_MIN_INTERVAL_SECONDS;
        }
    } else {
        snprintf(result_buf, result_sz, "Error: invalid type '%s'. Use periodic/daily/once", type_str);
        return false;
    }

    /* Create the cron entry */
    uint8_t id = cron_set(type, (uint32_t)interval_seconds, (uint8_t)minute, action);
    if (id == 0) {
        snprintf(result_buf, result_sz, "Error: failed to create scheduled task (no slots available)");
        return false;
    }

    /* Success response */
    char time_str[64];
    cron_get_time_str(time_str, sizeof(time_str));

    switch (type) {
        case CRON_TYPE_PERIODIC:
            if (interval_seconds >= 60) {
                snprintf(result_buf, result_sz,
                    "Scheduled task #%d created: will run every %d minute(s). Action: %.50s. Current time: %s",
                    id, interval_seconds / 60, action, time_str);
            } else {
                snprintf(result_buf, result_sz,
                    "Scheduled task #%d created: will run every %d second(s). Action: %.50s. Current time: %s",
                    id, interval_seconds, action, time_str);
            }
            break;
        case CRON_TYPE_DAILY:
            snprintf(result_buf, result_sz,
                "Scheduled task #%d created: will run daily at %02d:%02d. Action: %.50s. Current time: %s",
                id, interval_seconds, minute, action, time_str);
            break;
        case CRON_TYPE_ONCE:
            if (interval_seconds >= 60) {
                snprintf(result_buf, result_sz,
                    "One-time task #%d created: will run in %d minute(s). Action: %.50s. Current time: %s",
                    id, interval_seconds / 60, action, time_str);
            } else {
                snprintf(result_buf, result_sz,
                    "One-time task #%d created: will run in %d second(s). Action: %.50s. Current time: %s",
                    id, interval_seconds, action, time_str);
            }
            break;
        default:
            break;
    }

    return true;
}

/* -----------------------------------------------------------------------
 * Tool: cron_list
 * ----------------------------------------------------------------------- */
static bool tool_cron_list(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;

    char list_buf[1024];
    cron_list(list_buf, sizeof(list_buf));

    if (strlen(list_buf) <= 2) {  /* Empty array "[]" */
        char time_str[64];
        cron_get_time_str(time_str, sizeof(time_str));
        snprintf(result_buf, result_sz, "No scheduled tasks. Current time: %s", time_str);
    } else {
        snprintf(result_buf, result_sz, "Scheduled tasks: %s", list_buf);
    }

    return true;
}

/* -----------------------------------------------------------------------
 * Tool: cron_cancel
 * ----------------------------------------------------------------------- */
static bool tool_cron_cancel(const char *input_json, char *result_buf, size_t result_sz)
{
    int id = 0;
    if (!json_get_int(input_json, "id", &id) || id <= 0) {
        snprintf(result_buf, result_sz, "Error: 'id' required (positive integer)");
        return false;
    }

    esp_err_t err = cron_delete((uint8_t)id);
    if (err == ESP_ERR_NOT_FOUND) {
        snprintf(result_buf, result_sz, "Error: task #%d not found", id);
        return false;
    } else if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to cancel task #%d", id);
        return false;
    }

    snprintf(result_buf, result_sz, "Scheduled task #%d cancelled", id);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: cron_cancel_all
 * Cancel all scheduled tasks
 * ----------------------------------------------------------------------- */
static bool tool_cron_cancel_all(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;

    int count = cron_cancel_all();

    if (count == 0) {
        snprintf(result_buf, result_sz, "No scheduled tasks to cancel");
    } else {
        snprintf(result_buf, result_sz, "Cancelled %d scheduled task(s)", count);
    }

    return true;
}

/* -----------------------------------------------------------------------
 * Tool: get_time
 * ----------------------------------------------------------------------- */
static bool tool_get_time(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;

    char time_str[64];
    char tz_str[TIMEZONE_MAX_LEN];
    bool synced = cron_is_time_synced();

    cron_get_time_str(time_str, sizeof(time_str));
    cron_get_timezone(tz_str, sizeof(tz_str));

    snprintf(result_buf, result_sz,
        "Current time: %s\nTimezone: %s\nNTP synced: %s\nUnix timestamp: %lu",
        time_str, tz_str,
        synced ? "yes" : "no (waiting for sync)",
        (unsigned long)cron_get_unix_time());

    return true;
}

/* -----------------------------------------------------------------------
 * Tool: set_timezone
 * ----------------------------------------------------------------------- */
static bool tool_set_timezone(const char *input_json, char *result_buf, size_t result_sz)
{
    char tz_input[TIMEZONE_MAX_LEN] = {0};

    if (!json_get_str(input_json, "timezone", tz_input, sizeof(tz_input)) ||
        strlen(tz_input) == 0) {
        snprintf(result_buf, result_sz,
            "Error: 'timezone' required. Examples: UTC, Asia/Shanghai, America/New_York, or POSIX format like CST-8");
        return false;
    }

    /* Resolve alias to POSIX format */
    const char *tz_posix = resolve_timezone(tz_input);
    if (!tz_posix) {
        /* Try as-is, might be POSIX format */
        tz_posix = tz_input;
    }

    esp_err_t err = cron_set_timezone(tz_posix);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: failed to set timezone to '%s'", tz_input);
        return false;
    }

    snprintf(result_buf, result_sz, "Timezone set to %s (POSIX: %s)", tz_input, tz_posix);
    return true;
}
