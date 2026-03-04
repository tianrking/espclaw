/*
 * ESPClaw - channel/channel_lark.c
 * Lark/Feishu (飞书) Webhook channel — notification only (one-way).
 *
 * HTTPS POST to Lark/Feishu custom bot webhook.
 * Supports signature verification for security.
 * API: https://open.larksuite.com/document/client-docs/bot-v3/use-custom-bots-in-a-group
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

static const char *TAG = "lark";
static message_bus_t *s_bus = NULL;

/* Lark/Feishu configuration */
static char s_webhook_url[256] = "";
static char s_secret[128] = "";  /* Optional: for signature */

/* Output queue for Lark messages */
static QueueHandle_t s_lark_out_queue = NULL;

/* -----------------------------------------------------------------------
 * Inline Base64 encoding (same as dingtalk)
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

/* -----------------------------------------------------------------------
 * Configuration: load webhook URL and secret from NVS or Kconfig
 * ----------------------------------------------------------------------- */
static bool lark_load_config(void)
{
    /* Try NVS first */
    if (nvs_mgr_get_str("lark_webhook", s_webhook_url, sizeof(s_webhook_url)) == ESP_OK &&
        strlen(s_webhook_url) > 0) {
        ESP_LOGI(TAG, "Webhook loaded from NVS");
    } else {
#ifdef CONFIG_ESPCLAW_LARK_WEBHOOK
        strncpy(s_webhook_url, CONFIG_ESPCLAW_LARK_WEBHOOK, sizeof(s_webhook_url) - 1);
        ESP_LOGI(TAG, "Webhook from Kconfig");
#else
        return false;
#endif
    }

    /* Load secret (optional, for signing) */
    if (nvs_mgr_get_str("lark_secret", s_secret, sizeof(s_secret)) != ESP_OK) {
#ifdef CONFIG_ESPCLAW_LARK_SECRET
        strncpy(s_secret, CONFIG_ESPCLAW_LARK_SECRET, sizeof(s_secret) - 1);
#endif
    }

    return strlen(s_webhook_url) > 0;
}

/* -----------------------------------------------------------------------
 * Generate signature for Lark webhook security
 * Lark uses: timestamp + "\n" + secret -> HMAC-SHA256 -> Base64
 * ----------------------------------------------------------------------- */
static void lark_gen_signature(const char *secret, long long timestamp,
                                char *sig_out, size_t sig_sz)
{
    /* String to sign: timestamp + "\n" + secret */
    char string_to_sign[256];
    snprintf(string_to_sign, sizeof(string_to_sign), "%lld\n%s", timestamp, secret);

    /* HMAC-SHA256 */
    unsigned char hmac[32];
    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        (const unsigned char *)secret, strlen(secret),
        (const unsigned char *)string_to_sign, strlen(string_to_sign),
        hmac
    );

    /* Base64 encode */
    base64_encode(hmac, sizeof(hmac), sig_out, sig_sz);
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
 * Send message to Lark webhook
 * ----------------------------------------------------------------------- */
static esp_err_t lark_send_message(const char *text)
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

    /* Build JSON body */
    char body[4096];

    if (strlen(s_secret) > 0) {
        /* With signature */
        long long timestamp = (long long)(esp_timer_get_time() / 1000);
        char signature[128];
        lark_gen_signature(s_secret, timestamp, signature, sizeof(signature));

        snprintf(body, sizeof(body),
                 "{\"timestamp\":\"%lld\",\"sign\":\"%s\","
                 "\"msg_type\":\"text\",\"content\":{\"text\":\"%s\"}}",
                 timestamp, signature, escaped);
    } else {
        /* Without signature */
        snprintf(body, sizeof(body),
                 "{\"msg_type\":\"text\",\"content\":{\"text\":\"%s\"}}",
                 escaped);
    }

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

    /* Lark returns {"StatusCode":0,"StatusMessage":"success"} */
    if (strstr(resp, "\"StatusCode\":0") || strstr(resp, "\"code\":0")) {
        ESP_LOGI(TAG, "Message sent successfully");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Lark error: %s", resp);
        return ESP_FAIL;
    }
}

/* -----------------------------------------------------------------------
 * Lark output task: send messages from queue
 * ----------------------------------------------------------------------- */
static void lark_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_lark_out_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Sending: %.50s", msg.text);
            lark_send_message(msg.text);
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t lark_start(message_bus_t *bus)
{
    s_bus = bus;

    if (!lark_load_config()) {
        ESP_LOGW(TAG, "Lark disabled — no webhook configured");
        return ESP_ERR_NOT_FOUND;
    }

    /* Create output queue */
    s_lark_out_queue = xQueueCreate(4, sizeof(outbound_msg_t));
    if (!s_lark_out_queue) {
        return ESP_ERR_NO_MEM;
    }

    /* Start output task */
    xTaskCreate(lark_output_task, "lark_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Lark/Feishu webhook channel started");
    return ESP_OK;
}

static bool lark_is_available(void)
{
#ifdef CONFIG_ESPCLAW_LARK_WEBHOOK
    return strlen(CONFIG_ESPCLAW_LARK_WEBHOOK) > 0;
#else
    return false;
#endif
}

const channel_ops_t lark_channel_ops = {
    .name = "lark",
    .start = lark_start,
    .is_available = lark_is_available,
};

/* -----------------------------------------------------------------------
 * Public API: post message to Lark queue
 * ----------------------------------------------------------------------- */
void lark_post(const char *text)
{
    if (!s_lark_out_queue) return;

    outbound_msg_t msg = {0};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.target = MSG_SOURCE_LARK;
    msg.chat_id = 0;

    xQueueSend(s_lark_out_queue, &msg, pdMS_TO_TICKS(100));
}
