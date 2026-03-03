/*
 * ESPClaw - mem/nvs_manager.c
 * NVS read/write implementation.
 */
#include "nvs_manager.h"
#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "nvs_mgr";

esp_err_t nvs_mgr_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing...");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized");
    }
    return err;
}

bool nvs_mgr_get_str(const char *key, char *value, size_t max_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_str(handle, key, value, &max_len);
    nvs_close(handle);
    return (err == ESP_OK);
}

esp_err_t nvs_mgr_set_str(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

bool nvs_mgr_get_i32(const char *key, int32_t *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_i32(handle, key, value);
    nvs_close(handle);
    return (err == ESP_OK);
}

esp_err_t nvs_mgr_set_i32(const char *key, int32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_i32(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
