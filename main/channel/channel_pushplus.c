/*
 * ESPClaw - channel/channel_pushplus.c
 * Pushplus (推送加) channel — notification only (one-way).
 *
 * Simple HTTPS POST to pushplus API.
 * Supports WeChat, WeCom, DingTalk, Feishu, etc.
 * Website: http://www.pushplus.plus/
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

static const char *TAG = "pushplus";
static message_bus_t *s_bus = NULL;

/* Pushplus configuration */
static char s_token[64] = "";
static char s_topic[32] = "";  /* Optional: group push topic */

/* Output queue for Pushplus messages */
static QueueHandle_t s_pushplus_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Configuration: load token from NVS or Kconfig
 * ----------------------------------------------------------------------- */
static bool pushplus_load_config(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str("pushplus_token", s_token, sizeof(s_token)) == ESP_OK &&
        strlen(s_token) > 0) {
        ESP_LOGI(TAG, "Token loaded from NVS");
    } else {
#ifdef CONFIG_ESPCLAW_PUSHPLUS_TOKEN
        strncpy(s_token, CONFIG_ESPCLAW_PUSHPLUS_TOKEN, sizeof(s_token) - 1);
        ESP_LOGI(TAG, "Token from Kconfig");
#else
        return false;
#endif
    }

    /* Load optional topic */
    nvs_mgr_get_str("pushplus_topic", s_topic, sizeof(s_topic));

    return strlen(s_token) > 0;
}

/* -----------------------------------------------------------------------
 * HTTP response handler
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
 * Send message to Pushplus API
 * ----------------------------------------------------------------------- */
static esp_err_t pushplus_send_message(const char *text)
{
    char resp[512] = "";
    http_ctx_t ctx = { .buf = resp, .buf_sz = sizeof(resp) };

    /* Escape JSON string */
    char escaped[2048];
    size_t j = 0;
    for (const char *p = text; *p && j < sizeof(escaped) - 2; p++) {
        if (*p == '"' || *p == '\\') { escaped[j++] = '\\'; escaped[j++] = *p; }
        else if (*p == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else if (*p == '\r') { /* skip CR */ }
        else if ((unsigned char)*p >= 0x20) { escaped[j++] = *p; }
    }
    escaped[j] = '\0';

    /* Pushplus API JSON payload */
    char body[4096];
    if (strlen(s_topic) > 0) {
        snprintf(body, sizeof(body),
                 "{\"token\":\"%s\",\"title\":\"ESPClaw\",\"content\":\"%s\",\"topic\":\"%s\"}",
                 s_token, escaped, s_topic);
    } else {
        snprintf(body, sizeof(body),
                 "{\"token\":\"%s\",\"title\":\"ESPClaw\",\"content\":\"%s\"}",
                 s_token, escaped);
    }

    /* HTTP POST to pushplus API */
    esp_http_client_config_t cfg = {
        .url = "http://www.pushplus.plus/send",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "POST failed: status=%d", status);
        return ESP_FAIL;
    }

    /* Pushplus returns {"code":200,"msg":"success"} */
    if (strstr(resp, "\"code\":200")) {
        ESP_LOGI(TAG, "Message sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Pushplus error: %s", resp);
        return ESP_FAIL;
    }
}

/* -----------------------------------------------------------------------
 * Pushplus output task: send messages from queue
 * ----------------------------------------------------------------------- */
static void pushplus_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_pushplus_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Sending: %.50s", msg.text);
            pushplus_send_message(msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t pushplus_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!pushplus_load_config()) {
        ESP_LOGW(TAG, "Pushplus disabled — no token configured");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_pushplus_out_queue = xQueueCreate(4, sizeof(outbound_msg_t));
    if (!s_pushplus_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start output task */
    xTaskCreate(pushplus_output_task, "pushplus_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Pushplus channel started");
    return ESP_OK;
}

static bool pushplus_is_available(void)
{
#ifdef CONFIG_ESPCLAW_PUSHPLUS_TOKEN
    return strlen(CONFIG_ESPCLAW_PUSHPLUS_TOKEN) > 0;
#else
    return false;
#endif
}

const channel_ops_t pushplus_channel_ops = {
    .name = "pushplus",
    .start = pushplus_start,
    .is_available = pushplus_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to Pushplus queue
 * ----------------------------------------------------------------------- */
void pushplus_post(const char *text)
{
    if (!s_pushplus_out_queue) return;

    outbound_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.target = MSG_SOURCE_PUSHPLUS;
    msg.chat_id = 0;

    xQueueSend(s_pushplus_out_queue, &msg, pdMS_TO_TICKS(100));
}
