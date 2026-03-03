/*
 * ESPClaw - provider/llm_auth.c
 */
#include "llm_auth.h"
#include "mem/nvs_manager.h"
#include "nvs_keys.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "llm_auth";

esp_err_t llm_auth_load(llm_creds_t *creds)
{
    memset(creds, 0, sizeof(*creds));

    /* API key: NVS first, then Kconfig */
    if (!nvs_mgr_get_str(NVS_KEY_LLM_API_KEY, creds->api_key, sizeof(creds->api_key))) {
#ifdef CONFIG_ESPCLAW_LLM_API_KEY
        strncpy(creds->api_key, CONFIG_ESPCLAW_LLM_API_KEY, sizeof(creds->api_key) - 1);
#endif
    }

    /* Backend: NVS first, then Kconfig */
    int32_t backend = LLM_BACKEND_ANTHROPIC;
    if (!nvs_mgr_get_i32(NVS_KEY_LLM_BACKEND, &backend)) {
#ifdef CONFIG_ESPCLAW_LLM_BACKEND_INT
        backend = CONFIG_ESPCLAW_LLM_BACKEND_INT;
#endif
    }
    creds->backend = (int)backend;

    /* Model: NVS first, then Kconfig, then built-in default */
    if (!nvs_mgr_get_str(NVS_KEY_LLM_MODEL, creds->model, sizeof(creds->model))) {
#ifdef CONFIG_ESPCLAW_LLM_MODEL
        if (strlen(CONFIG_ESPCLAW_LLM_MODEL) > 0) {
            strncpy(creds->model, CONFIG_ESPCLAW_LLM_MODEL, sizeof(creds->model) - 1);
        } else
#endif
        switch (creds->backend) {
            case LLM_BACKEND_OPENAI:
                strncpy(creds->model, LLM_DEFAULT_MODEL_OPENAI, sizeof(creds->model) - 1);
                break;
            case LLM_BACKEND_OPENROUTER:
                strncpy(creds->model, LLM_DEFAULT_MODEL_OPENROUTER, sizeof(creds->model) - 1);
                break;
            case LLM_BACKEND_OLLAMA:
                strncpy(creds->model, LLM_DEFAULT_MODEL_OLLAMA, sizeof(creds->model) - 1);
                break;
            default:
                strncpy(creds->model, LLM_DEFAULT_MODEL_ANTHROPIC, sizeof(creds->model) - 1);
                break;
        }
    }

    /* Base URL: NVS first, then Kconfig */
    if (!nvs_mgr_get_str(NVS_KEY_LLM_API_URL, creds->base_url, sizeof(creds->base_url))) {
#ifdef CONFIG_ESPCLAW_LLM_BASE_URL
        if (strlen(CONFIG_ESPCLAW_LLM_BASE_URL) > 0)
            strncpy(creds->base_url, CONFIG_ESPCLAW_LLM_BASE_URL, sizeof(creds->base_url) - 1);
#endif
    }

    if (strlen(creds->api_key) == 0) {
        ESP_LOGE(TAG, "No API key. Set via menuconfig or provision.sh");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Backend: %d, model: %s", creds->backend, creds->model);
    return ESP_OK;
}
