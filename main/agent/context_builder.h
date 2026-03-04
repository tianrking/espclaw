/*
 * ESPClaw - agent/context_builder.h
 * System prompt assembly for the ReAct agent loop.
 * Step 5: static device-info prompt. Step 6 will inject tool descriptions.
 */
#ifndef CONTEXT_BUILDER_H
#define CONTEXT_BUILDER_H

#include "config.h"
#include <stddef.h>

/*
 * Build the system prompt into out[out_sz].
 * tools_desc: optional extra text appended after device info (NULL = skip).
 * Returns bytes written (excl. NUL), or -1 on truncation.
 */
int context_build_system_prompt(char *out, size_t out_sz,
                                 const char *tools_desc);

#endif /* CONTEXT_BUILDER_H */
