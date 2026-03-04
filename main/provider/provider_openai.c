/*
 * ESPClaw - provider/provider_openai.c
 * OpenAI-compatible API (also covers OpenRouter and Ollama).
 * POST /v1/chat/completions  {"model":..., "messages":[...]}
 */
#include "provider.h"
#include "config.h"
#include "tool/tool_registry.h"
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
    /* Skip null content (tool_calls response has "content":null) */
    const char *key = strstr(json, "\"content\":\"");
    if (!key) {
        /* Check for tool_calls — caller handles this case via finish_reason */
        strncpy(out, "[no content in response]", out_sz - 1);
        out[out_sz - 1] = '\0';
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

/*
 * Convert OpenAI tool_calls format to Anthropic-like format that
 * agent_loop's try_dispatch_tool() can parse.
 *
 * OpenAI response (finish_reason=tool_calls):
 *   {"choices":[{"message":{"tool_calls":[{"id":"call_xxx","function":
 *     {"name":"gpio_write","arguments":"{\"pin\":2,\"state\":1}"}}]}}]}
 *
 * We synthesize a string containing stop_reason=tool_use + id + name + input
 * so agent_loop can reuse the same parsing code.
 */
static void extract_tool_call(const char *json, char *out, size_t out_sz)
{
    /* Extract tool call id */
    char tool_id[64] = "unknown_id";
    const char *id_key = strstr(json, "\"id\":\"");
    if (id_key) {
        id_key += strlen("\"id\":\"");  /* skip "id":" and point to value start */
        size_t i = 0;
        while (*id_key && *id_key != '"' && i < sizeof(tool_id) - 1)
            tool_id[i++] = *id_key++;
        tool_id[i] = '\0';
    }

    /* Extract function name */
    char func_name[64] = "";
    const char *name_key = strstr(json, "\"name\":\"");
    if (name_key) {
        const char *start = name_key + strlen("\"name\":\"");
        size_t i = 0;
        while (*start && *start != '"' && i < sizeof(func_name) - 1)
            func_name[i++] = *start++;
        func_name[i] = '\0';
    }

    /* Extract arguments (already a JSON string, need to unescape) */
    char args[512] = "{}";
    const char *args_key = strstr(json, "\"arguments\":\"");
    if (args_key) {
        const char *start = args_key + strlen("\"arguments\":\"");
        size_t i = 0;
        while (*start && i < sizeof(args) - 1) {
            if (start[0] == '\\' && start[1] == '"') {
                args[i++] = '"'; start += 2;
            } else if (start[0] == '\\' && start[1] == '\\') {
                args[i++] = '\\'; start += 2;
            } else if (start[0] == '\\' && start[1] == 'n') {
                args[i++] = '\n'; start += 2;
            } else if (start[0] == '"') {
                break;
            } else {
                args[i++] = *start++;
            }
        }
        args[i] = '\0';
    }

    /*
     * Synthesize a string that try_dispatch_tool() can parse:
     * contains "stop_reason":"tool_use", "id", "name", "input"
     */
    snprintf(out, out_sz,
             "{\"stop_reason\":\"tool_use\","
             "\"id\":\"%s\","
             "\"name\":\"%s\","
             "\"input\":%s}",
             tool_id, func_name, args[0] ? args : "{}");
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
        /* JSON-escape the system prompt */
        len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
                        "{\"role\":\"system\",\"content\":\"");
        const char *sp = system_prompt;
        while (*sp && len < LLM_REQUEST_BUF_SIZE - 2) {
            unsigned char c = (unsigned char)*sp++;
            if      (c == '"')  { body[len++] = '\\'; body[len++] = '"';  }
            else if (c == '\\') { body[len++] = '\\'; body[len++] = '\\'; }
            else if (c == '\n') { body[len++] = '\\'; body[len++] = 'n';  }
            else if (c == '\r') { body[len++] = '\\'; body[len++] = 'r';  }
            else if (c == '\t') { body[len++] = '\\'; body[len++] = 't';  }
            else                { body[len++] = (char)c; }
        }
        body[len++] = '"';
        body[len++] = '}';
        body[len++] = ',';
        body[len]   = '\0';
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

    /* Close messages array */
    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len, "]");

    /* Build OpenAI-format tools JSON inline (ignoring tools_json arg which is Anthropic format) */
    char *oai_tools = malloc(LLM_REQUEST_BUF_SIZE / 2);
    if (oai_tools) {
        int tlen = tool_registry_build_tools_json_openai(oai_tools, LLM_REQUEST_BUF_SIZE / 2);
        if (tlen > 2) {
            len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len,
                ",\"tools\":%s", oai_tools);
        }
        free(oai_tools);
    }

    len += snprintf(body + len, LLM_REQUEST_BUF_SIZE - len, "}");

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

    /*
     * Step 6: detect tool_calls finish_reason (OpenAI format).
     * If found, synthesize Anthropic-like JSON so agent_loop can
     * reuse the same try_dispatch_tool() parsing code.
     */
    if (strstr(resp, "\"finish_reason\":\"tool_calls\"") ||
        strstr(resp, "\"tool_calls\":[")) {
        extract_tool_call(resp, response_buf, response_sz);
        ESP_LOGI(TAG, "tool_calls detected");
    } else {
        extract_content(resp, response_buf, response_sz);
        ESP_LOGI(TAG, "Got %d chars", (int)strlen(response_buf));
    }

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
