/*
 * ESPClaw - tool/tool_memory.c
 * NVS persistent memory tool handlers. Included by tool_registry.c.
 */
#include "mem/nvs_manager.h"
#include "util/json_util.h"
#include "config.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations */
static bool tool_memory_set(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_memory_get(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_memory_delete(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool tool_memory_list(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

/* -----------------------------------------------------------------------
 * Tool: memory_set
 * Store a value in persistent NVS memory.
 * ----------------------------------------------------------------------- */
static bool tool_memory_set(const char *input_json, char *result_buf, size_t result_sz)
{
    char key[NVS_MAX_KEY_LEN + 1] = {0};
    char value[NVS_MAX_VALUE_LEN + 1] = {0};

    if (!json_get_str(input_json, "key",   key,   sizeof(key))   || key[0] == '\0' ||
        !json_get_str(input_json, "value", value, sizeof(value))) {
        snprintf(result_buf, result_sz, "Error: missing key or value");
        return false;
    }
    if (strncmp(key, "u_", 2) != 0) {
        snprintf(result_buf, result_sz, "Error: key must start with u_");
        return false;
    }
    esp_err_t err = nvs_mgr_set_str(key, value);
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: NVS write failed (%s)", esp_err_to_name(err));
        return false;
    }
    snprintf(result_buf, result_sz, "Stored: %s = %s", key, value);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: memory_get
 * Retrieve a value from persistent NVS memory.
 * ----------------------------------------------------------------------- */
static bool tool_memory_get(const char *input_json, char *result_buf, size_t result_sz)
{
    char key[NVS_MAX_KEY_LEN + 1] = {0};
    if (!json_get_str(input_json, "key", key, sizeof(key)) || key[0] == '\0') {
        snprintf(result_buf, result_sz, "Error: missing key");
        return false;
    }
    if (strncmp(key, "u_", 2) != 0) {
        snprintf(result_buf, result_sz, "Error: key must start with u_");
        return false;
    }
    char value[NVS_MAX_VALUE_LEN + 1] = {0};
    bool found = nvs_mgr_get_str(key, value, sizeof(value));
    if (!found) {
        snprintf(result_buf, result_sz, "Not found: %s", key);
        return false;
    }
    snprintf(result_buf, result_sz, "%s = %s", key, value);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: memory_delete
 * Delete a key from persistent NVS memory.
 * ----------------------------------------------------------------------- */
static bool tool_memory_delete(const char *input_json, char *result_buf, size_t result_sz)
{
    char key[NVS_MAX_KEY_LEN + 1] = {0};
    if (!json_get_str(input_json, "key", key, sizeof(key)) || key[0] == '\0') {
        snprintf(result_buf, result_sz, "Error: missing key");
        return false;
    }
    if (strncmp(key, "u_", 2) != 0) {
        snprintf(result_buf, result_sz, "Error: key must start with u_");
        return false;
    }
    /* nvs_mgr_set_str with empty string signals deletion */
    esp_err_t err = nvs_mgr_set_str(key, "");
    if (err != ESP_OK) {
        snprintf(result_buf, result_sz, "Error: NVS delete failed (%s)", esp_err_to_name(err));
        return false;
    }
    snprintf(result_buf, result_sz, "Deleted: %s", key);
    return true;
}

/* -----------------------------------------------------------------------
 * Tool: memory_list
 * List all user memory keys (u_*).
 * ----------------------------------------------------------------------- */
static bool tool_memory_list(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;

    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(NVS_NAMESPACE, NULL, NVS_TYPE_ANY, &it);

    int count = 0;
    char *ptr = result_buf;
    size_t remaining = result_sz;

    int written = snprintf(ptr, remaining, "User memory keys:");
    if (written > 0 && (size_t)written < remaining) {
        ptr += written;
        remaining -= written;
    }

    while (err == ESP_OK && remaining > 30) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        /* Only show keys starting with u_ */
        if (strncmp(info.key, "u_", 2) == 0) {
            written = snprintf(ptr, remaining, "%s %s", count == 0 ? "" : ",", info.key);
            if (written > 0 && (size_t)written < remaining) {
                ptr += written;
                remaining -= written;
                count++;
            }
        }
        err = nvs_entry_next(&it);
    }

    if (it) {
        nvs_release_iterator(it);
    }

    if (count == 0) {
        snprintf(result_buf, result_sz, "No user memory keys found (keys must start with u_)");
    } else {
        snprintf(ptr, remaining, " (%d total)", count);
    }

    return true;
}
