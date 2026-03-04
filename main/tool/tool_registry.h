/*
 * ESPClaw - tool/tool_registry.h
 * Tool registry: registration, JSON schema building, dispatch.
 * Step 6: agent_loop calls tool_registry_dispatch() after detecting tool_use.
 */
#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include "tool.h"
#include <stddef.h>
#include <stdbool.h>

/* Call once at startup — registers all builtin tools */
void tool_registry_init(void);

/*
 * Build Anthropic-format tools JSON.
 * Format: [{"name":"...","description":"...","input_schema":{...}}, ...]
 * Returns bytes written, -1 on truncation.
 */
int tool_registry_build_tools_json(char *out, size_t out_sz);

/*
 * Build OpenAI-compatible tools JSON (for OpenAI / GLM-4 / OpenRouter).
 * Format: [{"type":"function","function":{"name":"...","description":"...","parameters":{...}}}]
 * Returns bytes written, -1 on truncation.
 */
int tool_registry_build_tools_json_openai(char *out, size_t out_sz);

/*
 * Dispatch a tool call by name.
 * name       : tool name from LLM response
 * input_json : "input" object from LLM tool_use block
 * result_buf : caller-allocated output buffer
 * result_sz  : size of result_buf
 * Returns true on success.
 */
bool tool_registry_dispatch(const char *name, const char *input_json,
                             char *result_buf, size_t result_sz);

/* Number of registered tools */
int tool_registry_count(void);

#endif /* TOOL_REGISTRY_H */
