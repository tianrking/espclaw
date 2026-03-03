/*
 * ESPClaw - provider/provider.h
 * LLM provider vtable interface.
 * Each backend (Anthropic, OpenAI, Ollama...) implements this struct.
 */
#ifndef PROVIDER_H
#define PROVIDER_H

#include "esp_err.h"
#include <stddef.h>

/*
 * complete() — send messages to the LLM, get text back.
 *
 * system_prompt : system prompt string (may be NULL)
 * messages_json : JSON array of {role, content} objects
 * tools_json    : JSON array of tool schemas (NULL if no tools yet)
 * response_buf  : caller-allocated buffer for assistant text
 * response_sz   : size of response_buf
 *
 * Returns ESP_OK on success. response_buf is NUL-terminated on success.
 */
typedef esp_err_t (*provider_complete_fn)(
    const char *system_prompt,
    const char *messages_json,
    const char *tools_json,
    char       *response_buf,
    size_t      response_sz
);

typedef struct {
    const char         *name;        /* "anthropic", "openai", etc. */
    esp_err_t         (*init)(const char *api_key, const char *model,
                              const char *base_url);
    provider_complete_fn complete;
    void              (*deinit)(void);
} provider_ops_t;

/* Registry — call before first use */
esp_err_t provider_registry_init(void);

/* Returns currently active provider, or NULL if not configured */
const provider_ops_t *provider_get_active(void);

/* Extern declarations for each backend */
extern const provider_ops_t anthropic_provider;
extern const provider_ops_t openai_provider;
extern const provider_ops_t openrouter_provider;
extern const provider_ops_t ollama_provider;

#endif /* PROVIDER_H */
