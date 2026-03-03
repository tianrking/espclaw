/*
 * ESPClaw - provider/provider_registry.c
 * Loads credentials from NVS and initialises the active provider.
 */
#include "provider.h"
#include "llm_auth.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "provider";

static const provider_ops_t *s_active = NULL;

esp_err_t provider_registry_init(void)
{
    llm_creds_t creds;
    esp_err_t err = llm_auth_load(&creds);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No LLM credentials — provider not initialised");
        return err;
    }

    const char *url = strlen(creds.base_url) > 0 ? creds.base_url : NULL;

    switch (creds.backend) {
        case LLM_BACKEND_OPENAI:
            s_active = &openai_provider;
            return openai_provider.init(creds.api_key, creds.model,
                                        url ? url : LLM_API_URL_OPENAI);
        case LLM_BACKEND_OPENROUTER:
            s_active = &openrouter_provider;
            return openrouter_provider.init(creds.api_key, creds.model,
                                            url ? url : LLM_API_URL_OPENROUTER);
        case LLM_BACKEND_OLLAMA:
            s_active = &ollama_provider;
            return ollama_provider.init(creds.api_key, creds.model,
                                        url ? url : LLM_API_URL_OLLAMA);
        case LLM_BACKEND_ANTHROPIC:
            s_active = &anthropic_provider;
            return anthropic_provider.init(creds.api_key, creds.model,
                                           url ? url : LLM_API_URL_ANTHROPIC);
        default: /* Custom or unknown — treat as OpenAI-compatible */
            s_active = &openai_provider;
            return openai_provider.init(creds.api_key, creds.model,
                                        url ? url : LLM_API_URL_OPENAI);
    }
}

const provider_ops_t *provider_get_active(void)
{
    return s_active;
}
