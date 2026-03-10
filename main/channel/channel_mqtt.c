/*
 * ESPClaw - channel/channel_mqtt.c
 * MQTT channel for IoT control — bidirectional communication via MQTT broker.
 *
 * Features:
 *   - Subscribe to command topic (espclaw/{client_id}/cmd)
 *   - Publish responses to topic (espclaw/{client_id}/response)
 *   - Optional TLS/SSL support
 *   - Last Will and Testament (LWT) for connection status
 *   - QoS 1 for reliable delivery
 *
 * Usage:
 *   1. Configure MQTT broker URL and credentials via menuconfig
 *   2. Publish commands to: espclaw/{device_id}/cmd
 *   3. Receive responses on: espclaw/{device_id}/response
 *   4. Monitor status on: espclaw/{device_id}/status (online/offline)
 */
#include "channel/channel.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "platform.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt_ch";

/* Runtime state */
static message_bus_t *s_bus = NULL;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static char s_client_id[32] = {0};
static char s_topic_cmd[64] = {0};
static char s_topic_response[64] = {0};
static char s_topic_status[64] = {0};
static bool s_connected = false;

/* Outbound queue for MQTT messages */
static QueueHandle_t s_mqtt_outbox = NULL;
#define MQTT_OUTBOX_SIZE        8
#define MQTT_MAX_PAYLOAD_LEN    2048

typedef struct {
    char topic[64];
    char payload[MQTT_MAX_PAYLOAD_LEN];
    int qos;
    bool retain;
} mqtt_out_msg_t;

/* -----------------------------------------------------------------------
 * Generate client ID from MAC address
 * ----------------------------------------------------------------------- */
static void mqtt_generate_client_id(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(s_client_id, sizeof(s_client_id), "espclaw_%02x%02x%02x",
             mac[3], mac[4], mac[5]);

    snprintf(s_topic_cmd, sizeof(s_topic_cmd),
             "espclaw/%s/cmd", s_client_id);
    snprintf(s_topic_response, sizeof(s_topic_response),
             "espclaw/%s/response", s_client_id);
    snprintf(s_topic_status, sizeof(s_topic_status),
             "espclaw/%s/status", s_client_id);

    ESP_LOGI(TAG, "Client ID: %s", s_client_id);
    ESP_LOGI(TAG, "Subscribe: %s", s_topic_cmd);
    ESP_LOGI(TAG, "Publish: %s", s_topic_response);
}

/* -----------------------------------------------------------------------
 * MQTT event handler
 * ----------------------------------------------------------------------- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            s_connected = true;

            /* Subscribe to command topic */
            int msg_id = esp_mqtt_client_subscribe(event->client, s_topic_cmd, 1);
            ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)", s_topic_cmd, msg_id);

            /* Publish online status */
            esp_mqtt_client_publish(event->client, s_topic_status,
                                    "online", 6, 1, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "Subscribed ack, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "Unsubscribed ack, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Published ack, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            /* Received message on subscribed topic */
            ESP_LOGI(TAG, "Received topic=%.*s, data=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            /* Forward to agent via inbound queue - wait up to 60s for agent to be ready */
            if (s_bus && event->data_len > 0 && event->data_len < MAX_MESSAGE_LEN) {
                inbound_msg_t msg = {0};
                memcpy(msg.text, event->data,
                       event->data_len < sizeof(msg.text) - 1 ?
                       event->data_len : sizeof(msg.text) - 1);
                msg.source = MSG_SOURCE_MQTT;
                msg.chat_id = 0;  /* MQTT doesn't use chat_id */

                /* Wait for agent to be ready (up to 60s to match LLM timeout) */
                if (message_bus_post_inbound(s_bus, &msg, pdMS_TO_TICKS(60000)) != pdPASS) {
                    ESP_LOGW(TAG, "Failed to post inbound message (queue full, timeout)");
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            if (event->error_handle) {
                ESP_LOGE(TAG, "MQTT error: type=%d, tls_err=%d",
                         event->error_handle->error_type,
                         event->error_handle->esp_tls_stack_err);
            } else {
                ESP_LOGE(TAG, "MQTT error: unknown");
            }
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT connecting...");
            break;

        default:
            ESP_LOGD(TAG, "MQTT event id=%d", event->event_id);
            break;
    }
}

/* -----------------------------------------------------------------------
 * MQTT publish task — handles outbound messages from agent
 * ----------------------------------------------------------------------- */
static void mqtt_publish_task(void *arg)
{
    mqtt_out_msg_t msg;

    while (1) {
        if (xQueueReceive(s_mqtt_outbox, &msg, portMAX_DELAY) == pdTRUE) {
            if (s_connected && s_mqtt_client) {
                int msg_id = esp_mqtt_client_publish(
                    s_mqtt_client,
                    msg.topic,
                    msg.payload,
                    0,  /* len=0 means strlen */
                    msg.qos,
                    msg.retain
                );
                ESP_LOGD(TAG, "Published to %s (msg_id=%d)", msg.topic, msg_id);
            } else {
                ESP_LOGW(TAG, "MQTT not connected, dropping message");
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t mqtt_start(message_bus_t *bus)
{
    s_bus = bus;

    /* Generate client ID and topics */
    mqtt_generate_client_id();

    /* Create outbound queue */
    s_mqtt_outbox = xQueueCreate(MQTT_OUTBOX_SIZE, sizeof(mqtt_out_msg_t));
    if (!s_mqtt_outbox) {
        ESP_LOGE(TAG, "Failed to create MQTT outbox queue");
        return ESP_ERR_NO_MEM;
    }

    /* Configure MQTT client */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_ESPCLAW_MQTT_BROKER_URL,
        .credentials.client_id = s_client_id,
    };

#ifdef CONFIG_ESPCLAW_MQTT_USERNAME
    if (strlen(CONFIG_ESPCLAW_MQTT_USERNAME) > 0) {
        mqtt_cfg.credentials.username = CONFIG_ESPCLAW_MQTT_USERNAME;
    }
#endif

#ifdef CONFIG_ESPCLAW_MQTT_PASSWORD
    if (strlen(CONFIG_ESPCLAW_MQTT_PASSWORD) > 0) {
        mqtt_cfg.credentials.authentication.password = CONFIG_ESPCLAW_MQTT_PASSWORD;
    }
#endif

    /* Configure LWT (Last Will and Testament) */
    mqtt_cfg.session.last_will.topic = s_topic_status;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.msg_len = 7;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    /* Create MQTT client */
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    /* Register event handler */
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    /* Start MQTT client */
    esp_err_t err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    /* Start publish task */
    xTaskCreate(mqtt_publish_task, "mqtt_pub",
                CHANNEL_TASK_STACK_SIZE, NULL,
                CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "MQTT channel started (broker: %s)",
             CONFIG_ESPCLAW_MQTT_BROKER_URL);
    return ESP_OK;
}

static bool mqtt_is_available(void)
{
    /* MQTT is available if broker URL is configured */
    return strlen(CONFIG_ESPCLAW_MQTT_BROKER_URL) > 0;
}

const channel_ops_t mqtt_channel_ops = {
    .name = "mqtt",
    .start = mqtt_start,
    .is_available = mqtt_is_available,
};

/* -----------------------------------------------------------------------
 * Public API — post message to MQTT
 * ----------------------------------------------------------------------- */
#ifdef CONFIG_ESPCLAW_CHANNEL_MQTT
void mqtt_post(const char *text)
{
    if (!s_mqtt_outbox || !text) {
        return;
    }

    mqtt_out_msg_t msg = {0};
    strncpy(msg.topic, s_topic_response, sizeof(msg.topic) - 1);
    strncpy(msg.payload, text, sizeof(msg.payload) - 1);
    msg.qos = 1;
    msg.retain = 0;

    if (xQueueSend(s_mqtt_outbox, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "MQTT outbox full, message dropped");
    }
}

/* Check if MQTT is connected */
bool mqtt_is_connected(void)
{
    return s_connected;
}

/* Get client ID */
const char *mqtt_get_client_id(void)
{
    return s_client_id;
}

/* Get command topic */
const char *mqtt_get_cmd_topic(void)
{
    return s_topic_cmd;
}

/* Get response topic */
const char *mqtt_get_response_topic(void)
{
    return s_topic_response;
}
#endif /* CONFIG_ESPCLAW_CHANNEL_MQTT */
