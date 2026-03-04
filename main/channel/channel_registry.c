/*
 * ESPClaw - channel/channel_registry.c
 * Enumerate and start all available channels.
 *
 * Channels are conditionally compiled based on Kconfig options.
 * Only enabled channels are included in the build.
 */
#include "channel/channel.h"
#include "platform.h"
#include "esp_log.h"

static const char *TAG = "ch_reg";

/* Extern declarations for channel vtables (conditionally compiled) */

/* --- Bidirectional channels --- */
#ifdef CONFIG_ESPCLAW_CHANNEL_SERIAL
extern const channel_ops_t serial_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_TELEGRAM
extern const channel_ops_t telegram_channel_ops;
#endif

/* --- One-way notification channels (webhook-based) --- */
#ifdef CONFIG_ESPCLAW_CHANNEL_DINGTALK
extern const channel_ops_t dingtalk_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_DISCORD
extern const channel_ops_t discord_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_SLACK
extern const channel_ops_t slack_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_WECOM
extern const channel_ops_t wecom_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_LARK
extern const channel_ops_t lark_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_PUSHPLUS
extern const channel_ops_t pushplus_channel_ops;
#endif

#ifdef CONFIG_ESPCLAW_CHANNEL_BARK
extern const channel_ops_t bark_channel_ops;
#endif

/* Channel registry array (conditionally populated) */
static const channel_ops_t *s_channels[] = {
    /* Bidirectional channels */
#ifdef CONFIG_ESPCLAW_CHANNEL_SERIAL
    &serial_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_TELEGRAM
    &telegram_channel_ops,
#endif

    /* One-way notification channels */
#ifdef CONFIG_ESPCLAW_CHANNEL_DINGTALK
    &dingtalk_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_DISCORD
    &discord_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_SLACK
    &slack_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_WECOM
    &wecom_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_LARK
    &lark_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_PUSHPLUS
    &pushplus_channel_ops,
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_BARK
    &bark_channel_ops,
#endif
};

void channel_registry_init(void)
{
    ESP_LOGI(TAG, "Channels registered: %d", (int)ARRAY_SIZE(s_channels));
}

esp_err_t channel_start_all(message_bus_t *bus)
{
    for (int i = 0; i < (int)ARRAY_SIZE(s_channels); i++) {
        if (s_channels[i]->is_available()) {
            ESP_LOGI(TAG, "Starting channel: %s", s_channels[i]->name);
            esp_err_t err = s_channels[i]->start(bus);
            if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Channel '%s' failed: %s",
                         s_channels[i]->name, esp_err_to_name(err));
                return err;
            }
        }
    }
    return ESP_OK;
}
