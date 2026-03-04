/*
 * ESPClaw - service/cron_service.c
 * Scheduled task service implementation.
 */
#include "service/cron_service.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "nvs_keys.h"
#include "mem/nvs_manager.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "cron";

/* Agent input queue for firing actions */
static QueueHandle_t s_agent_queue = NULL;

/* Cron entries */
static cron_entry_t s_entries[CRON_MAX_ENTRIES];
static bool s_time_synced = false;
static SemaphoreHandle_t s_entries_mutex = NULL;

/* Current timezone */
static char s_timezone[TIMEZONE_MAX_LEN] = DEFAULT_TIMEZONE_POSIX;

/* Next entry ID */
static uint8_t s_next_id = 1;

/* Pending actions buffer (avoid stack allocation in task) */
typedef struct {
    uint8_t id;
    char action[CRON_MAX_ACTION_LEN];
} pending_fire_t;
static pending_fire_t s_pending_fires[CRON_MAX_ENTRIES];

/* -----------------------------------------------------------------------
 * Mutex helpers
 * ----------------------------------------------------------------------- */
static bool entries_lock(TickType_t timeout_ticks)
{
    if (!s_entries_mutex) return false;
    return xSemaphoreTake(s_entries_mutex, timeout_ticks) == pdTRUE;
}

static void entries_unlock(void)
{
    if (s_entries_mutex) {
        xSemaphoreGive(s_entries_mutex);
    }
}

/* -----------------------------------------------------------------------
 * Timezone validation
 * ----------------------------------------------------------------------- */
static bool timezone_is_valid(const char *tz)
{
    if (!tz) return false;
    size_t len = strlen(tz);
    if (len == 0 || len >= TIMEZONE_MAX_LEN) return false;

    for (size_t i = 0; i < len; i++) {
        if (tz[i] < 0x20 || tz[i] == 0x7f) return false;
    }
    return true;
}

/* -----------------------------------------------------------------------
 * Apply timezone
 * ----------------------------------------------------------------------- */
static esp_err_t apply_timezone(const char *timezone_posix, bool persist)
{
    if (!timezone_is_valid(timezone_posix)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (setenv("TZ", timezone_posix, 1) != 0) {
        ESP_LOGE(TAG, "Failed to set TZ environment");
        return ESP_FAIL;
    }
    tzset();

    strncpy(s_timezone, timezone_posix, sizeof(s_timezone) - 1);
    s_timezone[sizeof(s_timezone) - 1] = '\0';
    ESP_LOGI(TAG, "Timezone applied: %s", s_timezone);

    if (persist) {
        nvs_mgr_set_str(NVS_KEY_TIMEZONE, s_timezone);
    }

    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Load timezone from NVS
 * ----------------------------------------------------------------------- */
static void load_timezone(void)
{
    char stored[TIMEZONE_MAX_LEN] = {0};
    if (nvs_mgr_get_str(NVS_KEY_TIMEZONE, stored, sizeof(stored)) == ESP_OK &&
        timezone_is_valid(stored)) {
        if (apply_timezone(stored, false) == ESP_OK) {
            return;
        }
    }

    ESP_LOGI(TAG, "Using default timezone: %s", DEFAULT_TIMEZONE_POSIX);
    apply_timezone(DEFAULT_TIMEZONE_POSIX, false);
}

/* -----------------------------------------------------------------------
 * NTP sync callback
 * ----------------------------------------------------------------------- */
static void time_sync_cb(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "NTP time synchronized");
    s_time_synced = true;

    /* Log current time */
    char time_str[64];
    cron_get_time_str(time_str, sizeof(time_str));
    ESP_LOGI(TAG, "Current time: %s", time_str);
}

/* -----------------------------------------------------------------------
 * Initialize SNTP
 * ----------------------------------------------------------------------- */
static void init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP (server: %s)", NTP_SERVER);

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    config.sync_cb = time_sync_cb;
    esp_netif_sntp_init(&config);
}

/* -----------------------------------------------------------------------
 * Load entries from NVS
 * ----------------------------------------------------------------------- */
static void load_entries(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_CRON, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    for (int i = 0; i < CRON_MAX_ENTRIES; i++) {
        char key[16];
        snprintf(key, sizeof(key), "entry_%d", i);

        size_t size = sizeof(cron_entry_t);
        if (nvs_get_blob(handle, key, &s_entries[i], &size) != ESP_OK) {
            s_entries[i].id = 0;  /* Mark as empty */
        } else {
            /* Track highest ID for next_id */
            if (s_entries[i].id >= s_next_id) {
                s_next_id = s_entries[i].id + 1;
            }
        }
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded cron entries from NVS");
}

/* -----------------------------------------------------------------------
 * Save single entry to NVS
 * ----------------------------------------------------------------------- */
static esp_err_t save_entry(int index)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_CRON, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open cron NVS: %s", esp_err_to_name(err));
        return err;
    }

    char key[16];
    snprintf(key, sizeof(key), "entry_%d", index);

    if (s_entries[index].id == 0) {
        err = nvs_erase_key(handle, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    } else {
        err = nvs_set_blob(handle, key, &s_entries[index], sizeof(cron_entry_t));
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save cron entry %d: %s", index, esp_err_to_name(err));
    }

    return err;
}

/* -----------------------------------------------------------------------
 * Initialize cron system
 * ----------------------------------------------------------------------- */
esp_err_t cron_init(void)
{
    memset(s_entries, 0, sizeof(s_entries));

    /* Create mutex */
    if (!s_entries_mutex) {
        s_entries_mutex = xSemaphoreCreateMutex();
        if (!s_entries_mutex) {
            ESP_LOGE(TAG, "Failed to create cron mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Load timezone */
    load_timezone();

    /* Load persisted entries */
    load_entries();

    /* Initialize SNTP */
    init_sntp();

    ESP_LOGI(TAG, "Cron service initialized");
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Cron background task
 * ----------------------------------------------------------------------- */
static void cron_task(void *arg)
{
    ESP_LOGI(TAG, "Cron task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CRON_CHECK_INTERVAL_MS));

        if (!s_time_synced) {
            ESP_LOGD(TAG, "Waiting for time sync...");
            continue;
        }

        time_t now;
        time(&now);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        int fire_count = 0;

        if (!entries_lock(pdMS_TO_TICKS(1000))) {
            ESP_LOGW(TAG, "Failed to lock entries for check");
            continue;
        }

        /* Check each entry */
        for (int i = 0; i < CRON_MAX_ENTRIES; i++) {
            cron_entry_t *e = &s_entries[i];
            if (e->id == 0 || !e->enabled) continue;

            bool should_fire = false;

            switch (e->type) {
                case CRON_TYPE_PERIODIC:
                    /* Fire if interval has passed */
                    if (e->last_run == 0 ||
                        (now - e->last_run) >= (time_t)e->interval_seconds) {
                        should_fire = true;
                    }
                    break;

                case CRON_TYPE_DAILY:
                    /* Fire if current time matches hour:minute and not yet run today */
                    if (tm_now.tm_hour == e->hour && tm_now.tm_min == e->minute) {
                        /* Check if already run this hour today */
                        struct tm tm_last;
                        time_t last = e->last_run;
                        localtime_r(&last, &tm_last);
                        if (last == 0 ||
                            tm_last.tm_yday != tm_now.tm_yday ||
                            tm_last.tm_year != tm_now.tm_year) {
                            should_fire = true;
                        }
                    }
                    break;

                case CRON_TYPE_ONCE:
                    /* Fire once after interval, then delete */
                    if (e->last_run == 0) {
                        e->last_run = now;  /* Set trigger time */
                        save_entry(i);
                    } else if ((now - e->last_run) >= (time_t)e->interval_seconds) {
                        should_fire = true;
                        e->id = 0;  /* Mark for deletion after firing */
                        save_entry(i);
                        ESP_LOGI(TAG, "ONCE task fired, auto-deleted");
                    }
                    break;
            }

            if (should_fire) {
                s_pending_fires[fire_count].id = e->id;
                strncpy(s_pending_fires[fire_count].action, e->action,
                        CRON_MAX_ACTION_LEN - 1);
                fire_count++;

                e->last_run = now;
                if (e->type != CRON_TYPE_ONCE) {
                    save_entry(i);
                }
            }
        }

        entries_unlock();

        /* Fire actions outside the lock */
        for (int i = 0; i < fire_count; i++) {
            ESP_LOGI(TAG, "Firing cron %d: %.50s",
                     s_pending_fires[i].id, s_pending_fires[i].action);

            inbound_msg_t msg = {0};
            strncpy(msg.text, s_pending_fires[i].action, sizeof(msg.text) - 1);
            msg.source = MSG_SOURCE_CRON;
            msg.chat_id = 0;

            if (xQueueSend(s_agent_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to queue cron action");
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Start cron service
 * ----------------------------------------------------------------------- */
esp_err_t cron_start(QueueHandle_t agent_input_queue)
{
    if (!agent_input_queue) {
        return ESP_ERR_INVALID_ARG;
    }
    s_agent_queue = agent_input_queue;

    /* Start cron task */
    BaseType_t ret = xTaskCreate(
        cron_task,
        "cron",
        CRON_TASK_STACK_SIZE,
        NULL,
        CRON_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create cron task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Cron service started");
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Add/update cron entry
 * ----------------------------------------------------------------------- */
uint8_t cron_set(cron_type_t type, uint32_t interval_seconds, uint8_t minute, const char *action)
{
    if (!action || strlen(action) == 0 || strlen(action) >= CRON_MAX_ACTION_LEN) {
        ESP_LOGW(TAG, "Invalid action for cron entry");
        return 0;
    }

    /* Validate minimum interval for periodic/once */
    if ((type == CRON_TYPE_PERIODIC || type == CRON_TYPE_ONCE) &&
        interval_seconds < CRON_MIN_INTERVAL_SECONDS) {
        ESP_LOGW(TAG, "Interval too small, using minimum %d seconds", CRON_MIN_INTERVAL_SECONDS);
        interval_seconds = CRON_MIN_INTERVAL_SECONDS;
    }

    if (!entries_lock(pdMS_TO_TICKS(1000))) {
        ESP_LOGW(TAG, "Failed to lock entries");
        return 0;
    }

    /* Find empty slot */
    int slot = -1;
    for (int i = 0; i < CRON_MAX_ENTRIES; i++) {
        if (s_entries[i].id == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        entries_unlock();
        ESP_LOGW(TAG, "No free cron slots");
        return 0;
    }

    /* Create entry */
    cron_entry_t *e = &s_entries[slot];
    memset(e, 0, sizeof(cron_entry_t));

    e->id = s_next_id++;
    if (s_next_id == 0) s_next_id = 1;  /* Wrap around, skip 0 */

    e->type = type;
    e->enabled = true;
    e->last_run = 0;

    switch (type) {
        case CRON_TYPE_PERIODIC:
        case CRON_TYPE_ONCE:
            e->interval_seconds = interval_seconds;
            break;
        case CRON_TYPE_DAILY:
            e->hour = (uint8_t)interval_seconds;  /* Reuse param for hour */
            e->minute = minute;
            break;
    }

    strncpy(e->action, action, CRON_MAX_ACTION_LEN - 1);

    save_entry(slot);
    entries_unlock();

    ESP_LOGI(TAG, "Created cron entry %d (type=%d, interval=%lus)", e->id, type, (unsigned long)interval_seconds);
    return e->id;
}

/* -----------------------------------------------------------------------
 * List entries as JSON
 * ----------------------------------------------------------------------- */
void cron_list(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) return;

    if (!entries_lock(pdMS_TO_TICKS(1000))) {
        snprintf(buf, buf_len, "[]");
        return;
    }

    int offset = snprintf(buf, buf_len, "[");

    for (int i = 0; i < CRON_MAX_ENTRIES && offset < (int)buf_len - 100; i++) {
        cron_entry_t *e = &s_entries[i];
        if (e->id == 0) continue;

        const char *type_str;
        switch (e->type) {
            case CRON_TYPE_PERIODIC: type_str = "periodic"; break;
            case CRON_TYPE_DAILY: type_str = "daily"; break;
            case CRON_TYPE_ONCE: type_str = "once"; break;
            default: type_str = "unknown";
        }

        offset += snprintf(buf + offset, buf_len - offset,
            "%s{\"id\":%d,\"type\":\"%s\",\"action\":\"%s\",\"enabled\":%s",
            (offset > 1) ? "," : "",
            e->id, type_str, e->action,
            e->enabled ? "true" : "false");

        if (e->type == CRON_TYPE_PERIODIC || e->type == CRON_TYPE_ONCE) {
            offset += snprintf(buf + offset, buf_len - offset,
                ",\"interval_seconds\":%lu", (unsigned long)e->interval_seconds);
        } else if (e->type == CRON_TYPE_DAILY) {
            offset += snprintf(buf + offset, buf_len - offset,
                ",\"hour\":%d,\"minute\":%d", e->hour, e->minute);
        }

        offset += snprintf(buf + offset, buf_len - offset, "}");
    }

    snprintf(buf + offset, buf_len - offset, "]");
    entries_unlock();
}

/* -----------------------------------------------------------------------
 * Delete entry
 * ----------------------------------------------------------------------- */
esp_err_t cron_delete(uint8_t id)
{
    if (id == 0) return ESP_ERR_INVALID_ARG;

    if (!entries_lock(pdMS_TO_TICKS(1000))) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < CRON_MAX_ENTRIES; i++) {
        if (s_entries[i].id == id) {
            s_entries[i].id = 0;
            save_entry(i);
            entries_unlock();
            ESP_LOGI(TAG, "Deleted cron entry %d", id);
            return ESP_OK;
        }
    }

    entries_unlock();
    return ESP_ERR_NOT_FOUND;
}

/* -----------------------------------------------------------------------
 * Cancel all entries
 * ----------------------------------------------------------------------- */
int cron_cancel_all(void)
{
    if (!entries_lock(pdMS_TO_TICKS(1000))) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < CRON_MAX_ENTRIES; i++) {
        if (s_entries[i].id != 0) {
            s_entries[i].id = 0;
            save_entry(i);
            count++;
        }
    }

    entries_unlock();
    ESP_LOGI(TAG, "Cancelled all %d cron entries", count);
    return count;
}

/* -----------------------------------------------------------------------
 * Get time string
 * ----------------------------------------------------------------------- */
void cron_get_time_str(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) return;

    time_t now;
    time(&now);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S %Z", &tm_now);
}

/* -----------------------------------------------------------------------
 * Set timezone
 * ----------------------------------------------------------------------- */
esp_err_t cron_set_timezone(const char *timezone_posix)
{
    return apply_timezone(timezone_posix, true);
}

/* -----------------------------------------------------------------------
 * Get timezone
 * ----------------------------------------------------------------------- */
void cron_get_timezone(char *buf, size_t buf_len)
{
    if (buf && buf_len > 0) {
        strncpy(buf, s_timezone, buf_len - 1);
        buf[buf_len - 1] = '\0';
    }
}

/* -----------------------------------------------------------------------
 * Is time synced
 * ----------------------------------------------------------------------- */
bool cron_is_time_synced(void)
{
    return s_time_synced;
}

/* -----------------------------------------------------------------------
 * Get Unix timestamp
 * ----------------------------------------------------------------------- */
uint32_t cron_get_unix_time(void)
{
    if (!s_time_synced) return 0;
    time_t now;
    time(&now);
    return (uint32_t)now;
}
