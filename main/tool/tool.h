/*
 * ESPClaw - tool/tool.h
 * Tool interface definition + capability bitmask.
 * Step 6: every tool implements tool_execute_fn; registry auto-filters by platform.
 */
#ifndef TOOL_H
#define TOOL_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Tool executor function signature.
 * input_json : the "input" JSON object from the LLM tool_use block (may be "{}")
 * result_buf : caller-allocated buffer for tool output text
 * result_sz  : size of result_buf
 * Returns true on success, false on error (still writes human-readable error to result_buf).
 */
typedef bool (*tool_execute_fn)(const char *input_json,
                                char       *result_buf,
                                size_t      result_sz);

typedef struct {
    const char       *name;               /* e.g. "gpio_write" */
    const char       *description;        /* human-readable */
    const char       *input_schema_json;  /* JSON Schema object string */
    tool_execute_fn   execute;
} tool_def_t;

#endif /* TOOL_H */
