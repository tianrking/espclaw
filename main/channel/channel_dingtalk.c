/*
 * ESPClaw - channel/channel_dingtalk.c
 * DingTalk (钉钉) Webhook channel — notification only (one-way).
 *
 * Step 7.5: HTTPS POST with HMAC-SHA256 signature.
 * No polling — this is a send-only channel for notifications.
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
#include "esp_timer.h"
#include "mbedtls/md.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * Inline Base64 encoding (avoid mbedtls dependency)
 * ----------------------------------------------------------------------- */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64_encode(const unsigned char *src, size_t len, char *dst, size_t dst_sz)
{
    size_t j = 0;
    for (size_t i = 0; i < len && j + 4 < dst_sz; i += 3) {
        unsigned int n = ((unsigned int)src[i]) << 16;
        if (i + 1 < len) n |= ((unsigned int)src[i+1]) << 8;
        if (i + 2 < len) n |= src[i+2];

        dst[j++] = b64_table[(n >> 18) & 0x3F];
        dst[j++] = b64_table[(n >> 12) & 0x3F];
        dst[j++] = (i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=';
        dst[j++] = (i + 2 < len) ? b64_table[n & 0x3F] : '=';
    }
    dst[j] = '\0';
    return j;
}

static const char *TAG = "dingtalk";
static message_bus_t *s_bus = NULL;

/* DingTalk webhook configuration */
static char s_webhook_url[256] = "";
static char s_secret[128] = "";

/* Output queue for DingTalk messages */
static QueueHandle_t s_dt_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Configuration: load webhook URL and secret from NVS or Kconfig
 * ----------------------------------------------------------------------- */
static bool dt_load_config(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str("dt_webhook", s_webhook_url, sizeof(s_webhook_url)) == ESP_OK &&
        strlen(s_webhook_url) > 0) {
        ESP_LOGI(TAG, "Webhook loaded from NVS");
    } else {
#ifdef CONFIG_ESPCLAW_DINGTALK_WEBHOOK
        strncpy(s_webhook_url, CONFIG_ESPCLAW_DINGTALK_WEBHOOK, sizeof(s_webhook_url) - 1);
        ESP_LOGI(TAG, "Webhook from Kconfig");
#else
        return false;
#endif
    }

    /* Load secret (optional, for signing) */
    if (nvs_mgr_get_str("dt_secret", s_secret, sizeof(s_secret)) != ESP_OK) {
#ifdef CONFIG_ESPCLAW_DINGTALK_SECRET
        strncpy(s_secret, CONFIG_ESPCLAW_DINGTALK_SECRET, sizeof(s_secret) - 1);
#endif
    }

    if (strlen(s_secret) > 0) {
        ESP_LOGI(TAG, "Secret configured (%d chars)", (int)strlen(s_secret));
    }

    return strlen(s_webhook_url) > 0;
}

/* -----------------------------------------------------------------------
 * HMAC-SHA256 signature for DingTalk security
 * ----------------------------------------------------------------------- */
static void dt_sign(const char *timestamp, const char *secret, char *out, size_t out_sz)
{
    /* String to sign: timestamp + "\n" + secret */
    char string_to_sign[256];
    snprintf(string_to_sign, sizeof(string_to_sign), "%s\n%s", timestamp, secret);

    /* HMAC-SHA256 */
    unsigned char hmac[32];
    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        (const unsigned char *)secret, strlen(secret),
        (const unsigned char *)string_to_sign, strlen(string_to_sign),
        hmac
    );

    /* Base64 encode using inline implementation */
    base64_encode(hmac, sizeof(hmac), out, out_sz);

    /* URL encode (replace + with %2B, / with %2F, = with %3D) */
    char encoded[256];
    char *p = encoded;
    for (char *s = out; *s && (p - encoded) < (int)sizeof(encoded) - 4; s++) {
        if (*s == '+') { *p++ = '%'; *p++ = '2'; *p++ = 'B'; }
        else if (*s == '/') { *p++ = '%'; *p++ = '2'; *p++ = 'F'; }
        else if (*s == '=') { *p++ = '%'; *p++ = '3'; *p++ = 'D'; }
        else { *p++ = *s; }
    }
    *p = '\0';
    strncpy(out, encoded, out_sz - 1);
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
 * Send message to DingTalk webhook
 * ----------------------------------------------------------------------- */
static esp_err_t dt_send_message(const char *text)
{
    char url[512];
    char resp[512] = "";
    http_ctx_t ctx = { .buf = resp, .buf_sz = sizeof(resp) };

    /* Build URL with signature if secret is configured */
    if (strlen(s_secret) > 0) {
        long long timestamp = (long long)(esp_timer_get_time() / 1000);
        char ts_str[32];
        snprintf(ts_str, sizeof(ts_str), "%lld", timestamp);
        char signature[256];
        dt_sign(ts_str, s_secret, signature, sizeof(signature));

        /* Append to webhook URL */
        char delim = strchr(s_webhook_url, '?') ? '&' : '?';
        snprintf(url, sizeof(url), "%s%ctimestamp=%s&sign=%s",
                 s_webhook_url, delim, ts_str, signature);
    } else {
        strncpy(url, s_webhook_url, sizeof(url) - 1);
    }

    /* Build JSON body */
    char escaped[2048];
    size_t j = 0;
    for (const char *p = text; *p && j < sizeof(escaped) - 2; p++) {
        if (*p == '"' || *p == '\\') { escaped[j++] = '\\'; escaped[j++] = *p; }
        else if (*p == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else if ((unsigned char)*p >= 0x20) { escaped[j++] = *p; }
    }
    escaped[j] = '\0';

    char body[4096];
    snprintf(body, sizeof(body),
             "{\"msgtype\":\"text\",\"text\":{\"content\":\"%s\"}}",
             escaped);

    /* HTTP POST */
    esp_http_client_config_t cfg = {
        .url = url,
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

    /* Check for errcode=0 in response */
    if (strstr(resp, "\"errcode\":0")) {
        ESP_LOGI(TAG, "Message sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "DingTalk error: %s", resp);
        return ESP_FAIL;
    }
}

/* -----------------------------------------------------------------------
 * DingTalk output task: send messages from queue
 * ----------------------------------------------------------------------- */
static void dt_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_dt_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Sending: %.50s", msg.text);
            dt_send_message(msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t dingtalk_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!dt_load_config()) {
        ESP_LOGW(TAG, "DingTalk disabled — no webhook configured");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_dt_out_queue = xQueueCreate(4, sizeof(outbound_msg_t));
    if (!s_dt_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start output task */
    xTaskCreate(dt_output_task, "dt_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "DingTalk webhook channel started");
    return ESP_OK;
}

static bool dingtalk_is_available(void)
{
#ifdef CONFIG_ESPCLAW_DINGTALK_WEBHOOK
    return strlen(CONFIG_ESPCLAW_DINGTALK_WEBHOOK) > 0;
#else
    return false;
#endif
}

const channel_ops_t dingtalk_channel_ops = {
    .name = "dingtalk",
    .start = dingtalk_start,
    .is_available = dingtalk_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to DingTalk queue
 * ----------------------------------------------------------------------- */
void dingtalk_post(const char *text)
{
    if (!s_dt_out_queue) return;

    outbound_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.target = MSG_SOURCE_DINGTALK;
    msg.chat_id = 0;

    xQueueSend(s_dt_out_queue, &msg, pdMS_TO_TICKS(100));
}
