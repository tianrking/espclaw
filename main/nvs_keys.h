/*
 * ESPClaw - nvs_keys.h
 * NVS key string constants for all persistent configuration.
 */
#ifndef NVS_KEYS_H
#define NVS_KEYS_H

/* WiFi */
#define NVS_KEY_WIFI_SSID        "wifi_ssid"
#define NVS_KEY_WIFI_PASS        "wifi_pass"

/* LLM */
#define NVS_KEY_LLM_BACKEND      "llm_backend"
#define NVS_KEY_LLM_API_KEY      "llm_api_key"
#define NVS_KEY_LLM_MODEL        "llm_model"
#define NVS_KEY_LLM_API_URL      "llm_api_url"

/* Telegram */
#define NVS_KEY_TG_TOKEN         "tg_token"
#define NVS_KEY_TG_CHAT_IDS      "tg_chat_ids"

/* Persona */
#define NVS_KEY_PERSONA          "persona"

/* Timezone */
#define NVS_KEY_TIMEZONE         "timezone"

/* Boot */
#define NVS_KEY_BOOT_COUNT       "boot_count"

/* Proxy (S3) */
#define NVS_KEY_PROXY_HOST       "proxy_host"
#define NVS_KEY_PROXY_PORT       "proxy_port"

/* Rate Limiting */
#define NVS_KEY_RL_DAILY         "rl_daily"
#define NVS_KEY_RL_DAY           "rl_day"
#define NVS_KEY_RL_YEAR          "rl_year"

#endif /* NVS_KEYS_H */
