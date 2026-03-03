/*
 * ESPClaw - bus/message_bus.h
 * FreeRTOS queue-based message bus for inter-task communication.
 */
#ifndef MESSAGE_BUS_H
#define MESSAGE_BUS_H

#include "messages.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

typedef struct {
    QueueHandle_t inbound;           /* channels -> agent */
    QueueHandle_t outbound;          /* agent -> channels */
} message_bus_t;

esp_err_t message_bus_init(message_bus_t *bus);

/* Post inbound message (channel -> agent) */
esp_err_t message_bus_post_inbound(message_bus_t *bus, const inbound_msg_t *msg,
                                    TickType_t timeout);

/* Post outbound message (agent -> channel) */
esp_err_t message_bus_post_outbound(message_bus_t *bus, const outbound_msg_t *msg,
                                     TickType_t timeout);

#endif /* MESSAGE_BUS_H */
