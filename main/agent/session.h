/*
 * ESPClaw - agent/session.h
 * Conversation history ring buffer for the agent loop.
 * Step 5: proper JSON escaping + tool_use/tool_result message types.
 */
#ifndef SESSION_H
#define SESSION_H

#include "messages.h"   /* conversation_msg_t, MAX_HISTORY_TURNS */
#include "esp_err.h"
#include <stddef.h>

typedef struct {
    conversation_msg_t turns[MAX_HISTORY_TURNS];
    int count;   /* valid entries (0 … MAX_HISTORY_TURNS) */
    int head;    /* index of oldest entry in ring buffer   */
} session_t;

/* Initialise / clear session */
void session_init(session_t *s);

/* Append a plain-text turn */
void session_append(session_t *s, const char *role, const char *content);

/* Append an assistant tool-use turn (Step 6 forward) */
void session_append_tool_use(session_t *s, const char *tool_id,
                              const char *tool_name, const char *input_json);

/* Append a tool-result turn (Step 6 forward) */
void session_append_tool_result(session_t *s, const char *tool_id,
                                 const char *result);

/* Remove the last message from session history (used when LLM fails) */
void session_pop_last(session_t *s);

/* Clear all session history */
void session_clear(session_t *s);

/*
 * Build Anthropic-format JSON messages array into out[out_sz].
 * Tool format: {"role":"assistant","content":[{"type":"tool_use",...}]}
 * Returns number of bytes written (excl. NUL), or -1 on truncation.
 */
int session_build_messages_json(const session_t *s, char *out, size_t out_sz);

/*
 * Build OpenAI-format JSON messages array into out[out_sz].
 * Tool format: {"role":"assistant","tool_calls":[{"type":"function",...}]}
 * Returns number of bytes written (excl. NUL), or -1 on truncation.
 */
int session_build_messages_json_openai(const session_t *s, char *out, size_t out_sz);

#endif /* SESSION_H */
