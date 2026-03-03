/*
 * ESPClaw - provider/llm_auth.h
 * API key and model loading from NVS / Kconfig fallback.
 */
#ifndef LLM_AUTH_H
#define LLM_AUTH_H

#include "config.h"
#include "esp_err.h"

typedef struct {
    char api_key[LLM_API_KEY_BUF_SIZE];
    char model[64];
    char base_url[128];   /* empty = use provider default */
    int  backend;         /* llm_backend_t */
} llm_creds_t;

/* Load credentials from NVS, fall back to Kconfig values */
esp_err_t llm_auth_load(llm_creds_t *creds);

#endif /* LLM_AUTH_H */
