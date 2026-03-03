/*
 * ESPClaw - bus/message_bus.c
 */
#include "message_bus.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "bus";

esp_err_t message_bus_init(message_bus_t *bus)
{
    bus->inbound = xQueueCreate(INPUT_QUEUE_LENGTH, sizeof(inbound_msg_t));
    bus->outbound = xQueueCreate(OUTPUT_QUEUE_LENGTH, sizeof(outbound_msg_t));

    if (!bus->inbound || !bus->outbound) {
        ESP_LOGE(TAG, "Failed to create message queues");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Message bus ready (in=%d, out=%d)",
             INPUT_QUEUE_LENGTH, OUTPUT_QUEUE_LENGTH);
    return ESP_OK;
}

esp_err_t message_bus_post_inbound(message_bus_t *bus, const inbound_msg_t *msg,
                                    TickType_t timeout)
{
    if (xQueueSend(bus->inbound, msg, timeout) != pdTRUE) {
        ESP_LOGW(TAG, "Inbound queue full, dropping message");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t message_bus_post_outbound(message_bus_t *bus, const outbound_msg_t *msg,
                                     TickType_t timeout)
{
    if (xQueueSend(bus->outbound, msg, timeout) != pdTRUE) {
        ESP_LOGW(TAG, "Outbound queue full, dropping message");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
