/*
 * ESPClaw - service/cron_service.h
 * Scheduled task service with NTP time sync and timezone support.
 *
 * Supports three types of scheduled tasks:
 * - PERIODIC: Run every N seconds (minimum 10s)
 * - DAILY: Run at specific hour:minute each day
 * - ONCE: Run once after N seconds (then auto-delete)
 */
#ifndef CRON_SERVICE_H
#define CRON_SERVICE_H

#include "config.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

/* Minimum interval in seconds (matches CRON_CHECK_INTERVAL_MS) */
#define CRON_MIN_INTERVAL_SECONDS   10

/* Cron entry types */
typedef enum {
    CRON_TYPE_PERIODIC,     /* Every N seconds */
    CRON_TYPE_DAILY,        /* At specific hour:minute */
    CRON_TYPE_ONCE,         /* Run once after N seconds */
} cron_type_t;

/* Cron entry structure */
typedef struct {
    uint8_t id;                         /* Unique ID (1-255, 0 = empty slot) */
    cron_type_t type;
    uint32_t interval_seconds;          /* For PERIODIC/ONCE: seconds */
    uint8_t hour;                       /* For DAILY: 0-23 */
    uint8_t minute;                     /* For DAILY: 0-59 */
    char action[CRON_MAX_ACTION_LEN];   /* Action text sent to agent */
    uint32_t last_run;                  /* Unix timestamp of last run */
    bool enabled;
} cron_entry_t;

/**
 * Initialize cron system.
 * - Loads persisted entries from NVS
 * - Syncs time via NTP
 * - Sets up timezone
 *
 * @return ESP_OK on success
 */
esp_err_t cron_init(void);

/**
 * Start cron background task.
 *
 * @param agent_input_queue Queue to send scheduled actions to
 * @return ESP_OK on success
 */
esp_err_t cron_start(QueueHandle_t agent_input_queue);

/**
 * Add or update a cron entry.
 *
 * @param type              Entry type (PERIODIC/DAILY/ONCE)
 * @param interval_seconds  For PERIODIC/ONCE: interval in seconds (minimum 10)
 *                          For DAILY: hour (0-23)
 * @param minute            For DAILY: minute (0-59), ignored otherwise
 * @param action            Action text to execute
 * @return Entry ID on success, 0 on error
 */
uint8_t cron_set(cron_type_t type, uint32_t interval_seconds, uint8_t minute, const char *action);

/**
 * List all cron entries as JSON array.
 *
 * @param buf     Output buffer
 * @param buf_len Buffer size
 */
void cron_list(char *buf, size_t buf_len);

/**
 * Delete a cron entry by ID.
 *
 * @param id Entry ID to delete
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t cron_delete(uint8_t id);

/**
 * Cancel all cron entries.
 *
 * @return Number of entries cancelled
 */
int cron_cancel_all(void);

/**
 * Get current time as formatted string.
 *
 * @param buf     Output buffer
 * @param buf_len Buffer size
 */
void cron_get_time_str(char *buf, size_t buf_len);

/**
 * Set timezone using POSIX TZ string.
 *
 * @param timezone_posix POSIX timezone (e.g., "UTC0", "PST8PDT")
 * @return ESP_OK on success
 */
esp_err_t cron_set_timezone(const char *timezone_posix);

/**
 * Get configured timezone string.
 *
 * @param buf     Output buffer
 * @param buf_len Buffer size
 */
void cron_get_timezone(char *buf, size_t buf_len);

/**
 * Check if time has been synchronized via NTP.
 *
 * @return true if time is synced
 */
bool cron_is_time_synced(void);

/**
 * Get current Unix timestamp.
 *
 * @return Unix timestamp, or 0 if not synced
 */
uint32_t cron_get_unix_time(void);

#endif /* CRON_SERVICE_H */
