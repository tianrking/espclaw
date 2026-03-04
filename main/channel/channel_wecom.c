/*
 * ESPClaw - channel/channel_wecom.c
 * 企业微信 Webhook channel — notification only (one-way).
 *
 * HTTPS POST to WeCom group robot webhook.
 * Supports markdown formatting.
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

static const char *TAG = "wecom";
static message_bus_t *s_bus = NULL;

/* WeCom webhook configuration */
static char s_webhook_url[256] = "";

/* Output queue for WeCom messages */
static QueueHandle_t s_wecom_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Configuration: load webhook URL from NVS or Kconfig
 * ----------------------------------------------------------------------- */
static bool wecom_load_config(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str("wecom_webhook", s_webhook_url, sizeof(s_webhook_url)) == ESP_OK &&
        strlen(s_webhook_url) > 0) {
        ESP_LOGI(TAG, "Webhook loaded from NVS");
        return true;
    }

#ifdef CONFIG_ESPCLAW_WECOM_WEBHOOK
    strncpy(s_webhook_url, CONFIG_ESPCLAW_WECOM_WEBHOOK, sizeof(s_webhook_url) - 1);
    if (strlen(s_webhook_url) > 0) {
        ESP_LOGI(TAG, "Webhook from Kconfig");
        return true;
    }
#endif

    return false;
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
 * Send message to WeCom webhook
 * ----------------------------------------------------------------------- */
static esp_err_t wecom_send_message(const char *text)
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

    /* WeCom webhook JSON payload (markdown format) */
    char body[4096];
    snprintf(body, sizeof(body),
             "{\"msgtype\":\"markdown\",\"markdown\":{\"content\":\"%s\"}}",
             escaped);

    /* HTTP POST */
    esp_http_client_config_t cfg = {
        .url = s_webhook_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
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

    /* WeCom returns {"errcode":0,"errmsg":"ok"} on success */
    if (strstr(resp, "\"errcode\":0")) {
        ESP_LOGI(TAG, "Message sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "WeCom error: %s", resp);
        return ESP_FAIL;
    }
}

/* -----------------------------------------------------------------------
 * WeCom output task: send messages from queue
 * ----------------------------------------------------------------------- */
static void wecom_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_wecom_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Sending: %.50s", msg.text);
            wecom_send_message(msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t wecom_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!wecom_load_config()) {
        ESP_LOGW(TAG, "WeCom disabled — no webhook configured");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_wecom_out_queue = xQueueCreate(4, sizeof(outbound_msg_t));
    if (!s_wecom_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start output task */
    xTaskCreate(wecom_output_task, "wecom_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "WeCom webhook channel started");
    return ESP_OK;
}

static bool wecom_is_available(void)
{
#ifdef CONFIG_ESPCLAW_WECOM_WEBHOOK
    return strlen(CONFIG_ESPCLAW_WECOM_WEBHOOK) > 0;
#else
    return false;
#endif
}

const channel_ops_t wecom_channel_ops = {
    .name = "wecom",
    .start = wecom_start,
    .is_available = wecom_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to WeCom queue
 * ----------------------------------------------------------------------- */
void wecom_post(const char *text)
{
    if (!s_wecom_out_queue) return;

    outbound_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.target = MSG_SOURCE_WECOM;
    msg.chat_id = 0;

    xQueueSend(s_wecom_out_queue, &msg, pdMS_TO_TICKS(100));
}
