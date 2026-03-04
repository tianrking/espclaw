/*
 * ESPClaw - channel/channel_bark.c
 * Bark iOS Push channel — notification only (one-way).
 *
 * Simple HTTP GET/POST to Bark API for iOS push notifications.
 * Bark is a third-party iOS app that provides custom push URLs.
 * App Store: https://apps.apple.com/app/bark-customed-notifications/id1403753865
 * GitHub: https://github.com/Finb/Bark
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

static const char *TAG = "bark";
static message_bus_t *s_bus = NULL;

/* Bark configuration */
static char s_server[128] = "https://api.day.app";  /* Default: official server */
static char s_key[64] = "";  /* Device key from Bark app */

/* Output queue for Bark messages */
static QueueHandle_t s_bark_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Configuration: load server and key from NVS or Kconfig
 * ----------------------------------------------------------------------- */
static bool bark_load_config(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str("bark_key", s_key, sizeof(s_key)) == ESP_OK &&
        strlen(s_key) > 0) {
        ESP_LOGI(TAG, "Key loaded from NVS");
    } else {
#ifdef CONFIG_ESPCLAW_BARK_KEY
        strncpy(s_key, CONFIG_ESPCLAW_BARK_KEY, sizeof(s_key) - 1);
        ESP_LOGI(TAG, "Key from Kconfig");
#else
        return false;
#endif
    }

    /* Load optional custom server */
    nvs_mgr_get_str("bark_server", s_server, sizeof(s_server));

    return strlen(s_key) > 0;
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
 * URL encode for GET request
 * ----------------------------------------------------------------------- */
static void url_encode(const char *src, char *dst, size_t dst_sz)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;

    for (const char *p = src; *p && j < dst_sz - 4; p++) {
        if (isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            dst[j++] = *p;
        } else if (*p == ' ') {
            dst[j++] = '%';
            dst[j++] = '2';
            dst[j++] = '0';
        } else {
            dst[j++] = '%';
            dst[j++] = hex[(*p >> 4) & 0xF];
            dst[j++] = hex[*p & 0xF];
        }
    }
    dst[j] = '\0';
}

/* -----------------------------------------------------------------------
 * Send message to Bark API
 * ----------------------------------------------------------------------- */
static esp_err_t bark_send_message(const char *text)
{
    char resp[512] = "";
    http_ctx_t ctx = { .buf = resp, .buf_sz = sizeof(resp) };

    /* Build URL: https://api.day.app/KEY/TITLE/BODY */
    char encoded[1024];
    url_encode(text, encoded, sizeof(encoded));

    char url[512];
    snprintf(url, sizeof(url), "%s/%s/ESPClaw/%s",
             s_server, s_key, encoded);

    /* HTTP GET */
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .buffer_size = 512,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "GET failed: status=%d", status);
        return ESP_FAIL;
    }

    /* Bark returns {"code":200,"message":"success"} */
    if (strstr(resp, "\"code\":200")) {
        ESP_LOGI(TAG, "Push sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Bark error: %s", resp);
        return ESP_FAIL;
    }
}

/* -----------------------------------------------------------------------
 * Bark output task: send messages from queue
 * ----------------------------------------------------------------------- */
static void bark_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_bark_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Sending: %.50s", msg.text);
            bark_send_message(msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t bark_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!bark_load_config()) {
        ESP_LOGW(TAG, "Bark disabled — no key configured");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_bark_out_queue = xQueueCreate(4, sizeof(outbound_msg_t));
    if (!s_bark_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start output task */
    xTaskCreate(bark_output_task, "bark_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Bark push channel started (server: %s)", s_server);
    return ESP_OK;
}

static bool bark_is_available(void)
{
#ifdef CONFIG_ESPCLAW_BARK_KEY
    return strlen(CONFIG_ESPCLAW_BARK_KEY) > 0;
#else
    return false;
#endif
}

const channel_ops_t bark_channel_ops = {
    .name = "bark",
    .start = bark_start,
    .is_available = bark_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to Bark queue
 * ----------------------------------------------------------------------- */
void bark_post(const char *text)
{
    if (!s_bark_out_queue) return;

    outbound_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.target = MSG_SOURCE_BARK;
    msg.chat_id = 0;

    xQueueSend(s_bark_out_queue, &msg, pdMS_TO_TICKS(100));
}
