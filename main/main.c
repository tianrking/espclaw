/*
 * ESPClaw - main.c (Step 6: tool system)
 *
 * What this does:
 *   1. Print banner
 *   2. Init NVS
 *   3. Connect WiFi
 *   4. Init LLM provider (from NVS/menuconfig)
 *   5. Init GPIO HAL
 *   6. Init message bus + start serial channel
 *   7. Start agent loop (ReAct: calls LLM, dispatches tools, loops)
 *
 * Verify: idf.py build flash monitor
 * Try typing:
 *   espclaw> what is 2+2?   → LLM replies
 *   espclaw> heap           → Free heap: XXXXX bytes
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "platform.h"
#include "config.h"
#include "mem/nvs_manager.h"
#include "net/wifi_manager.h"
#include "bus/message_bus.h"
#include "channel/channel.h"
#include "provider/provider.h"
#include "agent/agent_loop.h"
#include "hal/hal_gpio.h"
#include "service/cron_service.h"
#include "util/ratelimit.h"

static const char *TAG = "espclaw";
static message_bus_t s_bus;

void app_main(void)
{
    /* 1. Banner */
    printf("\n");
    printf("=============================\n");
    printf("  ESPClaw v0.1.0\n");
    printf("  Target: %s\n", ESPCLAW_TARGET_NAME);
#if ESPCLAW_DUAL_CORE
    printf("  Mode: dual-core + PSRAM\n");
#else
    printf("  Mode: single-core\n");
#endif
    printf("=============================\n\n");

    /* 2. NVS init */
    esp_err_t err = nvs_mgr_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }

    /* 2.5 Rate limiter init */
    ratelimit_init();

    /* 2.6 Cron service init */
    err = cron_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cron init failed: %s", esp_err_to_name(err));
    }

    /* 3. WiFi connect (non-fatal) */
    err = wifi_mgr_init_and_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi not connected. Serial-only mode.");
    }

    /* 4. LLM provider (non-fatal: agent will show a helpful error per message) */
    err = provider_registry_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LLM not configured — set API key via menuconfig or provision.sh");
    }

    /* 5. GPIO HAL */
    hal_gpio_init();

    /* 6. Message bus */
    err = message_bus_init(&s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Message bus init failed");
        return;
    }

    /* 6. Start agent */
    err = agent_start(&s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Agent start failed");
        return;
    }

    /* 6.5 Start cron service (needs agent queue for firing actions) */
    err = cron_start(s_bus.inbound);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cron service start failed: %s", esp_err_to_name(err));
    }

    /* 7. Start channels (serial CLI, later: Telegram, WebSocket) */
    channel_registry_init();
    channel_start_all(&s_bus);

    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "ESPClaw ready");
}
