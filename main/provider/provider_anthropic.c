/*
 * ESPClaw - provider/provider_anthropic.c
 * Anthropic Messages API implementation.
 * POST https://api.anthropic.com/v1/messages
 */
#include "provider.h"
#include "config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "anthropic";

/* State shared between HTTP event handler and caller */
typedef struct {
    char  *buf;
    size_t buf_sz;
    size_t written;
    bool   truncated;
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
        if ((size_t)evt->data_len > remaining) {
            ctx->truncated = true;
        }
    }
    return ESP_OK;
}

/* Extract text from: {"content":[{"type":"text","text":"..."}],...} */
static void extract_text(const char *json, char *out, size_t out_sz)
{
    /* Find "text":" after "type":"text" */
    const char *type_text = strstr(json, "\"type\":\"text\"");
    if (!type_text) {
        /* Fallback: grab first "text":" occurrence */
        type_text = json;
    }
    const char *text_key = strstr(type_text, "\"text\":\"");
    if (!text_key) {
        strncpy(out, "[no text in response]", out_sz - 1);
        return;
    }
    const char *start = text_key + strlen("\"text\":\"");
    size_t pos = 0;
    while (*start && pos < out_sz - 1) {
        if (start[0] == '\\' && start[1] == '"') {
            out[pos++] = '"';
            start += 2;
        } else if (start[0] == '\\' && start[1] == 'n') {
            out[pos++] = '\n';
            start += 2;
        } else if (start[0] == '\\' && start[1] == '\\') {
            out[pos++] = '\\';
            start += 2;
        } else if (start[0] == '"') {
            break;  /* end of string */
        } else {
            out[pos++] = *start++;
        }
    }
    out[pos] = '\0';
}

static char   s_api_key[LLM_API_KEY_BUF_SIZE];
static char   s_model[64];
static char   s_base_url[128];

static esp_err_t anthropic_init(const char *api_key, const char *model,
                                const char *base_url)
{
    strncpy(s_api_key, api_key, sizeof(s_api_key) - 1);
    strncpy(s_model,   model,   sizeof(s_model)   - 1);
    if (base_url && strlen(base_url) > 0)
        strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    else
        strncpy(s_base_url, LLM_API_URL_ANTHROPIC, sizeof(s_base_url) - 1);
    return ESP_OK;
}

static esp_err_t anthropic_complete(
    const char *system_prompt,
    const char *messages_json,
    const char *tools_json,
    char       *response_buf,
    size_t      response_sz)
{
    /* Build JSON body into a heap buffer */
    char *body = malloc(LLM_REQUEST_BUF_SIZE);
    if (!body) return ESP_ERR_NO_MEM;

    int len = 0;
    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
        "{\"model\":\"%s\",\"max_tokens\":%d",
        s_model, LLM_MAX_TOKENS);

    if (system_prompt && strlen(system_prompt) > 0) {
        len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
            ",\"system\":\"%s\"", system_prompt);
    }

    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
        ",\"messages\":%s}", messages_json ? messages_json : "[]");

    /* HTTP response buffer */
    char *resp = malloc(LLM_RESPONSE_BUF_SIZE);
    if (!resp) { free(body); return ESP_ERR_NO_MEM; }

    http_ctx_t ctx = { .buf = resp, .buf_sz = LLM_RESPONSE_BUF_SIZE };

    esp_http_client_config_t cfg = {
        .url             = s_base_url,
        .method          = HTTP_METHOD_POST,
        .timeout_ms      = LLM_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler   = http_event_handler,
        .user_data       = &ctx,
        .buffer_size     = 2048,
        .buffer_size_tx  = LLM_REQUEST_BUF_SIZE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { free(body); free(resp); return ESP_FAIL; }

    /* Headers */
    char auth_header[LLM_API_KEY_BUF_SIZE + 16];
    snprintf(auth_header, sizeof(auth_header), "%s", s_api_key);
    esp_http_client_set_header(client, "x-api-key", auth_header);
    esp_http_client_set_header(client, "anthropic-version", "2023-06-01");
    esp_http_client_set_header(client, "content-type", "application/json");

    esp_http_client_set_post_field(client, body, len);

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP error: %s", esp_err_to_name(err));
        free(resp);
        return err;
    }

    if (status != 200) {
        ESP_LOGE(TAG, "API status %d: %.200s", status, resp);
        free(resp);
        return ESP_FAIL;
    }

    if (ctx.truncated) {
        ESP_LOGW(TAG, "Response buffer truncated");
    }

    extract_text(resp, response_buf, response_sz);
    free(resp);

    ESP_LOGI(TAG, "Got %d chars", (int)strlen(response_buf));
    return ESP_OK;
}

const provider_ops_t anthropic_provider = {
    .name    = "anthropic",
    .init    = anthropic_init,
    .complete = anthropic_complete,
    .deinit  = NULL,
};
