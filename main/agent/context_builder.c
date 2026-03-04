/*
 * ESPClaw - agent/context_builder.c
 * Assembles the system prompt: identity + device info + optional tool list.
 */
#include "context_builder.h"
#include "platform.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>

int context_build_system_prompt(char *out, size_t out_sz, const char *tools_desc)
{
    int n = snprintf(out, out_sz,
        "You are ESPClaw, an embedded AI assistant running on an %s microcontroller "
        "(ESP-IDF 5.5, FreeRTOS). Free heap: %lu bytes. "
        "Be concise. Respond in the same language the user uses. "
        "IMPORTANT: When the user asks you to remember, store, or save anything, "
        "you MUST call the memory_set tool immediately. Key must start with u_. "
        "When the user asks what you remember or stored, call memory_get. "
        "Never just say you remembered something without actually calling the tool.",
        ESPCLAW_TARGET_NAME,
        (unsigned long)esp_get_free_heap_size());

    if (n <= 0 || (size_t)n >= out_sz) return -1;

    if (tools_desc && tools_desc[0]) {
        int n2 = snprintf(out + n, out_sz - (size_t)n,
                          "\n\nAvailable tools:\n%s", tools_desc);
        if (n2 > 0 && (size_t)n2 < out_sz - (size_t)n)
            n += n2;
    }

    return n;
}
