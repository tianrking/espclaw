/*
 * ESPClaw - channel/channel_telegram.c
 * Telegram Bot channel — long polling getUpdates + sendMessage.
 *
 * Step 7: HTTPS long poll, message parsing, chat ID whitelist.
 */
#include "channel/channel.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "nvs_keys.h"
#include "mem/nvs_manager.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "telegram";
static message_bus_t *s_bus = NULL;

/* Telegram credentials (loaded from NVS or Kconfig) */
static char s_token[128] = "";
static char s_chat_ids[256] = "";  /* comma-separated whitelist */

/* Output queue for Telegram responses */
static QueueHandle_t s_tg_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Token & Chat ID management
 * ----------------------------------------------------------------------- */
static bool tg_load_credentials(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str(NVS_KEY_TG_TOKEN, s_token, sizeof(s_token)) == ESP_OK &&
        strlen(s_token) > 0) {
        ESP_LOGI(TAG, "Token loaded from NVS (%d chars)", (int)strlen(s_token));
    } else {
#ifdef CONFIG_ESPCLAW_TELEGRAM_TOKEN
        strncpy(s_token, CONFIG_ESPCLAW_TELEGRAM_TOKEN, sizeof(s_token) - 1);
        ESP_LOGI(TAG, "Token from Kconfig");
#else
        ESP_LOGW(TAG, "No Telegram token configured");
        return false;
#endif
    }

    /* Load chat ID whitelist (optional) */
    if (nvs_mgr_get_str(NVS_KEY_TG_CHAT_IDS, s_chat_ids, sizeof(s_chat_ids)) != ESP_OK) {
#ifdef CONFIG_ESPCLAW_TG_CHAT_IDS
        strncpy(s_chat_ids, CONFIG_ESPCLAW_TG_CHAT_IDS, sizeof(s_chat_ids) - 1);
#endif
    }

    if (strlen(s_chat_ids) > 0) {
        ESP_LOGI(TAG, "Chat ID whitelist: %s", s_chat_ids);
    } else {
        ESP_LOGI(TAG, "No chat ID whitelist — accepting all chats");
    }

    return strlen(s_token) > 0;
}

static bool tg_chat_allowed(int64_t chat_id)
{
    if (strlen(s_chat_ids) == 0) {
        return true;  /* No whitelist = allow all */
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)chat_id);

    /* Check if chat_id is in whitelist */
    return strstr(s_chat_ids, buf) != NULL;
}

/* -----------------------------------------------------------------------
 * HTTP response buffer
 * ----------------------------------------------------------------------- */
typedef struct {
    char *buf;
    size_t buf_sz;
    size_t written;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
        size_t remaining = ctx->buf_sz - ctx->written - 1;
        size_t to_copy = (evt->data_len < (int)remaining) ? (size_t)evt->data_len : remaining;
        if (to_copy > 0) {
            memcpy(ctx->buf + ctx->written, evt->data, to_copy);
            ctx->written += to_copy;
            ctx->buf[ctx->written] = '\0';
        }
    }
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Telegram API helpers
 * ----------------------------------------------------------------------- */
static esp_err_t tg_http_get(const char *url, char *resp, size_t resp_sz)
{
    http_ctx_t ctx = { .buf = resp, .buf_sz = resp_sz };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 35000,  /* Long poll timeout + margin */
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .buffer_size = 4096,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "HTTP %d: %s", status, esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t tg_http_post(const char *url, const char *body, char *resp, size_t resp_sz)
{
    http_ctx_t ctx = { .buf = resp, .buf_sz = resp_sz };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .buffer_size = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "POST HTTP %d", status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * JSON parsing (lightweight, no cJSON)
 * ----------------------------------------------------------------------- */

/* Extract integer value for a key */
static bool json_get_int64(const char *json, const char *key, int64_t *out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;

    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t')) p++;

    *out = strtoll(p, NULL, 10);
    return true;
}

/* Extract string value for a key (unescaped, up to maxlen) */
static bool json_get_string(const char *json, const char *key, char *out, size_t maxlen)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;

    p += strlen(search);
    size_t i = 0;
    while (*p && *p != '"' && i < maxlen - 1) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case '\\': out[i++] = '\\'; break;
                case '"': out[i++] = '"'; break;
                default: out[i++] = *p; break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return i > 0;
}

/* Escape string for JSON */
static size_t json_escape(const char *src, char *dst, size_t dst_sz)
{
    size_t j = 0;
    for (const char *p = src; *p && j < dst_sz - 2; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if (c >= 0x20) {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
    return j;
}

/* -----------------------------------------------------------------------
 * Telegram API: getUpdates (long poll)
 * ----------------------------------------------------------------------- */
static int tg_get_updates(int64_t offset, char *resp, size_t resp_sz)
{
    char url[256];
    snprintf(url, sizeof(url),
             "%s%s/getUpdates?timeout=%d&offset=%lld",
             TELEGRAM_API_URL, s_token, TELEGRAM_POLL_TIMEOUT, (long long)offset);

    esp_err_t err = tg_http_get(url, resp, resp_sz);
    if (err != ESP_OK) {
        return -1;
    }

    /* Check for "ok":true */
    if (!strstr(resp, "\"ok\":true")) {
        ESP_LOGW(TAG, "getUpdates not ok: %s", resp);
        return -1;
    }

    return (int)strlen(resp);
}

/* -----------------------------------------------------------------------
 * Telegram API: sendMessage
 * ----------------------------------------------------------------------- */
static esp_err_t tg_send_message(int64_t chat_id, const char *text)
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s/sendMessage", TELEGRAM_API_URL, s_token);

    char escaped[TELEGRAM_MAX_MSG_LEN];
    json_escape(text, escaped, sizeof(escaped));

    char body[TELEGRAM_MAX_MSG_LEN + 128];
    snprintf(body, sizeof(body),
             "{\"chat_id\":%lld,\"text\":\"%s\",\"parse_mode\":\"Markdown\"}",
             (long long)chat_id, escaped);

    char resp[512];
    return tg_http_post(url, body, resp, sizeof(resp));
}

/* -----------------------------------------------------------------------
 * Parse updates and dispatch to agent
 * ----------------------------------------------------------------------- */
static int64_t tg_parse_and_dispatch(const char *json, int64_t last_offset)
{
    /* Find "result":[ array */
    const char *result = strstr(json, "\"result\":[");
    if (!result) return last_offset;

    const char *p = result + 10;  /* skip "result":[ */

    while (*p) {
        /* Find each update object */
        const char *update_start = strstr(p, "{\"update_id\":");
        if (!update_start) break;

        const char *update_end = strchr(update_start, '}');
        if (!update_end) break;

        /* Extract update_id */
        int64_t update_id = 0;
        if (json_get_int64(update_start, "update_id", &update_id)) {
            last_offset = update_id + 1;
        }

        /* Look for message object */
        const char *msg_obj = strstr(update_start, "\"message\":{");
        if (msg_obj && msg_obj < update_end) {
            /* Extract chat_id from "chat":{"id":...} */
            const char *chat_id_key = strstr(msg_obj, "\"id\":");
            int64_t chat_id = 0;
            if (chat_id_key) {
                chat_id_key += 5;
                chat_id = strtoll(chat_id_key, NULL, 10);
            }

            /* Extract text */
            char text[CHANNEL_RX_BUF_SIZE] = "";
            if (json_get_string(msg_obj, "text", text, sizeof(text))) {
                /* Check whitelist */
                if (!tg_chat_allowed(chat_id)) {
                    ESP_LOGW(TAG, "Unauthorized chat_id: %lld", (long long)chat_id);
                    tg_send_message(chat_id, "Unauthorized. Contact admin.");
                } else {
                    ESP_LOGI(TAG, "Rx from %lld: %s", (long long)chat_id, text);

                    /* Post to inbound queue */
                    inbound_msg_t msg = {0};
                    strncpy(msg.text, text, sizeof(msg.text) - 1);
                    msg.source = MSG_SOURCE_TELEGRAM;
                    msg.chat_id = chat_id;
                    message_bus_post_inbound(s_bus, &msg, pdMS_TO_TICKS(100));
                }
            }
        }

        p = update_end + 1;
    }

    return last_offset;
}

/* -----------------------------------------------------------------------
 * Telegram output task: send replies from agent
 * ----------------------------------------------------------------------- */
static void tg_output_task(void *arg)
{
    telegram_msg_t msg;

    while (1) {
        if (xQueueReceive(s_tg_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Tx to %lld: %s", (long long)msg.chat_id, msg.text);
            tg_send_message(msg.chat_id, msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Telegram poll task: long poll getUpdates loop
 * ----------------------------------------------------------------------- */
static void tg_poll_task(void *arg)
{
    char *resp = malloc(LLM_RESPONSE_BUF_SIZE);
    if (!resp) {
        ESP_LOGE(TAG, "OOM for response buffer");
        vTaskDelete(NULL);
        return;
    }

    int64_t last_offset = 0;

#if TELEGRAM_FLUSH_ON_START
    /* Flush pending updates on startup */
    ESP_LOGI(TAG, "Flushing pending updates...");
    while (tg_get_updates(last_offset, resp, LLM_RESPONSE_BUF_SIZE) > 0) {
        if (strstr(resp, "\"result\":[]")) break;
        last_offset = tg_parse_and_dispatch(resp, last_offset);
    }
    ESP_LOGI(TAG, "Flush complete, starting poll loop");
#endif

    while (1) {
        int len = tg_get_updates(last_offset, resp, LLM_RESPONSE_BUF_SIZE);
        if (len > 0) {
            last_offset = tg_parse_and_dispatch(resp, last_offset);
        } else {
            /* Error or empty — wait before retry */
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t telegram_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!tg_load_credentials()) {
        ESP_LOGW(TAG, "Telegram disabled — no token");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_tg_out_queue = xQueueCreate(TELEGRAM_OUTPUT_QUEUE_LENGTH, sizeof(telegram_msg_t));
    if (!s_tg_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start poll task */
    xTaskCreate(tg_poll_task, "tg_poll", TG_POLL_TASK_STACK_SIZE,
                NULL, TG_POLL_TASK_PRIORITY, NULL);

    /* Start output task */
    xTaskCreate(tg_output_task, "tg_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Telegram bot started");
    return ESP_OK;
}

static bool telegram_is_available(void)
{
    /* Check if token is configured */
#ifdef CONFIG_ESPCLAW_TELEGRAM_TOKEN
    return strlen(CONFIG_ESPCLAW_TELEGRAM_TOKEN) > 0;
#else
    return false;
#endif
}

const channel_ops_t telegram_channel_ops = {
    .name = "telegram",
    .start = telegram_start,
    .is_available = telegram_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to Telegram output queue
 * ----------------------------------------------------------------------- */
void telegram_post_outbound(int64_t chat_id, const char *text)
{
    if (!s_tg_out_queue) return;

    telegram_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.chat_id = chat_id;

    xQueueSend(s_tg_out_queue, &msg, pdMS_TO_TICKS(100));
}
