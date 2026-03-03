/*
 * ESPClaw - channel/channel.h
 * Channel vtable interface. Inspired by zeroclaw's Channel trait.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include "bus/message_bus.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct channel_ops {
    const char *name;                           /* "serial", "telegram", etc */
    esp_err_t (*start)(message_bus_t *bus);      /* Start channel tasks */
    bool      (*is_available)(void);            /* Runtime check */
} channel_ops_t;

/* Channel registry */
void channel_registry_init(void);
esp_err_t channel_start_all(message_bus_t *bus);

#endif /* CHANNEL_H */
