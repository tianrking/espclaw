/*
 * ESPClaw - provider/provider_openai.c
 * OpenAI-compatible API (also covers OpenRouter and Ollama).
 * POST /v1/chat/completions  {"model":..., "messages":[...]}
 */
#include "provider.h"
#include "config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "openai";

typedef struct {
    char  *buf;
    size_t buf_sz;
    size_t written;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
        size_t remaining = ctx->buf_sz - ctx->written - 1;
        size_t to_copy   = (evt->data_len < (int)remaining)
                           ? (size_t)evt->data_len : remaining;
        if (to_copy > 0) {
            memcpy(ctx->buf + ctx->written, evt->data, to_copy);
            ctx->written += to_copy;
            ctx->buf[ctx->written] = '\0';
        }
    }
    return ESP_OK;
}

/* Extract from: {"choices":[{"message":{"content":"..."}}]} */
static void extract_content(const char *json, char *out, size_t out_sz)
{
    const char *key = strstr(json, "\"content\":\"");
    if (!key) {
        strncpy(out, "[no content in response]", out_sz - 1);
        return;
    }
    const char *start = key + strlen("\"content\":\"");
    size_t pos = 0;
    while (*start && pos < out_sz - 1) {
        if (start[0] == '\\' && start[1] == '"') {
            out[pos++] = '"';  start += 2;
        } else if (start[0] == '\\' && start[1] == 'n') {
            out[pos++] = '\n'; start += 2;
        } else if (start[0] == '\\' && start[1] == '\\') {
            out[pos++] = '\\'; start += 2;
        } else if (start[0] == '"') {
            break;
        } else {
            out[pos++] = *start++;
        }
    }
    out[pos] = '\0';
}

static char s_api_key[LLM_API_KEY_BUF_SIZE];
static char s_model[64];
static char s_base_url[128];
static bool s_bearer_auth;  /* OpenAI/OpenRouter use Bearer, Ollama has no auth */

static esp_err_t openai_init(const char *api_key, const char *model,
                             const char *base_url)
{
    strncpy(s_api_key,  api_key,  sizeof(s_api_key)  - 1);
    strncpy(s_model,    model,    sizeof(s_model)     - 1);
    strncpy(s_base_url, base_url && strlen(base_url) > 0
                        ? base_url : LLM_API_URL_OPENAI,
            sizeof(s_base_url) - 1);
    s_bearer_auth = (strlen(s_api_key) > 0);
    return ESP_OK;
}

static esp_err_t openai_complete(
    const char *system_prompt,
    const char *messages_json,
    const char *tools_json,
    char       *response_buf,
    size_t      response_sz)
{
    char *body = malloc(LLM_REQUEST_BUF_SIZE);
    if (!body) return ESP_ERR_NO_MEM;

    int len = 0;
    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
        "{\"model\":\"%s\",\"max_tokens\":%d,\"messages\":[",
        s_model, LLM_MAX_TOKENS);

    if (system_prompt && strlen(system_prompt) > 0) {
        len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
            "{\"role\":\"system\",\"content\":\"%s\"},", system_prompt);
    }

    /* Append user messages (strip outer brackets from messages_json) */
    if (messages_json && strlen(messages_json) > 2) {
        /* messages_json is "[{...},{...}]" — copy inner content */
        const char *inner = messages_json + 1;
        size_t inner_len  = strlen(inner) - 1; /* drop trailing ] */
        if (inner_len > 0 && len + (int)inner_len < LLM_REQUEST_BUF_SIZE - 4) {
            memcpy(body + len, inner, inner_len);
            len += inner_len;
        }
    }

    /* Close messages array and object */
    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len, "]}");

    char *resp = malloc(LLM_RESPONSE_BUF_SIZE);
    if (!resp) { free(body); return ESP_ERR_NO_MEM; }

    http_ctx_t ctx = { .buf = resp, .buf_sz = LLM_RESPONSE_BUF_SIZE };

    esp_http_client_config_t cfg = {
        .url              = s_base_url,
        .method           = HTTP_METHOD_POST,
        .timeout_ms       = LLM_HTTP_TIMEOUT_MS,
        .crt_bundle_attach= esp_crt_bundle_attach,
        .event_handler    = http_event_handler,
        .user_data        = &ctx,
        .buffer_size      = 2048,
        .buffer_size_tx   = LLM_REQUEST_BUF_SIZE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { free(body); free(resp); return ESP_FAIL; }

    esp_http_client_set_header(client, "content-type", "application/json");
    if (s_bearer_auth) {
        char auth_val[LLM_API_KEY_BUF_SIZE + 8];
        snprintf(auth_val, sizeof(auth_val), "Bearer %s", s_api_key);
        esp_http_client_set_header(client, "authorization", auth_val);
    }

    esp_http_client_set_post_field(client, body, len);

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "HTTP %d: %s, status=%d", err, esp_err_to_name(err), status);
        free(resp);
        return ESP_FAIL;
    }

    extract_content(resp, response_buf, response_sz);
    free(resp);
    return ESP_OK;
}

const provider_ops_t openai_provider = {
    .name     = "openai",
    .init     = openai_init,
    .complete = openai_complete,
    .deinit   = NULL,
};

/* OpenRouter and Ollama share the same wire format — reuse with different defaults */
const provider_ops_t openrouter_provider = {
    .name     = "openrouter",
    .init     = openai_init,
    .complete = openai_complete,
    .deinit   = NULL,
};

const provider_ops_t ollama_provider = {
    .name     = "ollama",
    .init     = openai_init,
    .complete = openai_complete,
    .deinit   = NULL,
};
