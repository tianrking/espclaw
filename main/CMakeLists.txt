# ESPClaw main component
# Add sources incrementally as modules are implemented.

set(SRCS
    "main.c"
    "mem/nvs_manager.c"
    "net/wifi_manager.c"
    "bus/message_bus.c"
    "util/text_buffer.c"
    "channel/channel_serial.c"
    "channel/channel_registry.c"
    # Step 4: LLM provider
    "provider/llm_auth.c"
    "provider/provider_registry.c"
    "provider/provider_anthropic.c"
    "provider/provider_openai.c"
    # Step 4: Agent loop
    "agent/agent_loop.c"
    # Step 5: ReAct agent modules
    "agent/session.c"
    "agent/context_builder.c"
    # Step 6: Tool system
    "util/json_util.c"
    "hal/hal_gpio.c"
    "tool/tool_registry.c"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash esp_event esp_timer esp_netif
        esp_wifi driver
        esp_http_client esp-tls mbedtls
)
