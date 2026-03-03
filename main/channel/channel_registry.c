/*
 * ESPClaw - channel/channel_registry.c
 * Enumerate and start all available channels.
 */
#include "channel/channel.h"
#include "platform.h"
#include "esp_log.h"

static const char *TAG = "ch_reg";

/* Extern declarations for channel vtables */
extern const channel_ops_t serial_channel_ops;
/* Future: extern const channel_ops_t telegram_channel_ops; */

static const channel_ops_t *s_channels[] = {
    &serial_channel_ops,
    /* Future: &telegram_channel_ops, */
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
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Channel '%s' failed: %s",
                         s_channels[i]->name, esp_err_to_name(err));
                return err;
            }
        }
    }
    return ESP_OK;
}
