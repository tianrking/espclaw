/*
 * ESPClaw - channel/channel.h
 * Channel vtable interface. Inspired by zeroclaw's Channel trait.
 *
 * Channels are conditionally compiled based on Kconfig options.
 * Only enabled channels are included in the build.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include "bus/message_bus.h"
#include "esp_err.h"
#include <stdbool.h>

/* Channel operations vtable */
typedef struct channel_ops {
    const char *name;                           /* "serial", "telegram", etc */
    esp_err_t (*start)(message_bus_t *bus);      /* Start channel tasks */
    bool      (*is_available)(void);            /* Runtime check */
} channel_ops_t;

/* Channel registry */
void channel_registry_init(void);
esp_err_t channel_start_all(message_bus_t *bus);

/* =========================================================================
 * Channel-specific public APIs (conditionally declared)
 * ========================================================================= */

/* --- Bidirectional channels --- */

#ifdef CONFIG_ESPCLAW_CHANNEL_SERIAL
void serial_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_TELEGRAM
void telegram_post(const char *text, int64_t chat_id);
#endif

/* --- One-way notification channels (webhook-based) --- */

#ifdef CONFIG_ESPCLAW_CHANNEL_DINGTALK
void dingtalk_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_DISCORD
void discord_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_SLACK
void slack_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_WECOM
void wecom_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_LARK
void lark_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_PUSHPLUS
void pushplus_post(const char *text);
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_BARK
void bark_post(const char *text);
#endif

#endif /* CHANNEL_H */
