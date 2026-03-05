# ESPClaw main component
# Add sources incrementally as modules are implemented.
#
# Channel sources are conditionally compiled based on Kconfig options.
# Only enabled channels are included to save flash and RAM.

set(SRCS
    "main.c"
    "mem/nvs_manager.c"
    "net/wifi_manager.c"
    "bus/message_bus.c"
    "util/text_buffer.c"
    "channel/channel_registry.c"
    # LLM provider
    "provider/llm_auth.c"
    "provider/provider_registry.c"
    "provider/provider_anthropic.c"
    "provider/provider_openai.c"
    # Agent loop
    "agent/agent_loop.c"
    # ReAct agent modules
    "agent/session.c"
    "agent/context_builder.c"
    "agent/persona.c"
    # Tool system
    "util/json_util.c"
    "hal/hal_gpio.c"
    "hal/hal_pwm.c"
    "hal/hal_adc.c"
    "hal/hal_servo.c"
    "hal/hal_onewire.c"
    "tool/tool_registry.c"
    # Services
    "service/cron_service.c"
    # Utilities
    "util/ratelimit.c"
)

# =========================================================================
# Conditionally add channel sources based on Kconfig
# =========================================================================

# --- Bidirectional channels ---
if(CONFIG_ESPCLAW_CHANNEL_SERIAL)
    list(APPEND SRCS "channel/channel_serial.c")
    message(STATUS "ESPClaw: Serial channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_TELEGRAM)
    list(APPEND SRCS "channel/channel_telegram.c")
    message(STATUS "ESPClaw: Telegram channel enabled")
endif()

# --- One-way notification channels (webhook-based) ---
if(CONFIG_ESPCLAW_CHANNEL_DINGTALK)
    list(APPEND SRCS "channel/channel_dingtalk.c")
    message(STATUS "ESPClaw: DingTalk channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_DISCORD)
    list(APPEND SRCS "channel/channel_discord.c")
    message(STATUS "ESPClaw: Discord channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_SLACK)
    list(APPEND SRCS "channel/channel_slack.c")
    message(STATUS "ESPClaw: Slack channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_WECOM)
    list(APPEND SRCS "channel/channel_wecom.c")
    message(STATUS "ESPClaw: WeCom channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_LARK)
    list(APPEND SRCS "channel/channel_lark.c")
    message(STATUS "ESPClaw: Lark/Feishu channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_PUSHPLUS)
    list(APPEND SRCS "channel/channel_pushplus.c")
    message(STATUS "ESPClaw: Pushplus channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_BARK)
    list(APPEND SRCS "channel/channel_bark.c")
    message(STATUS "ESPClaw: Bark channel enabled")
endif()

if(CONFIG_ESPCLAW_CHANNEL_MQTT)
    list(APPEND SRCS "channel/channel_mqtt.c")
    message(STATUS "ESPClaw: MQTT channel enabled")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash esp_event esp_timer esp_netif
        esp_wifi driver esp_adc
        esp_http_client esp-tls mbedtls
        mqtt
)
