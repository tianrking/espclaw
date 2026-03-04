/*
 * ESPClaw - agent/session.c
 * Conversation history ring buffer with correct JSON escaping.
 * Step 5: fixes the double-write bug in Step 4's build_messages_json.
 */
#include "session.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

void session_init(session_t *s)
{
    memset(s, 0, sizeof(*s));
}

/* Internal: get pointer to next write slot, advance ring */
static conversation_msg_t *session_alloc(session_t *s)
{
    int idx;
    if (s->count < MAX_HISTORY_TURNS) {
        idx = s->count++;
    } else {
        /* Overwrite oldest */
        idx = s->head;
        s->head = (s->head + 1) % MAX_HISTORY_TURNS;
    }
    conversation_msg_t *m = &s->turns[idx];
    memset(m, 0, sizeof(*m));
    return m;
}

void session_append(session_t *s, const char *role, const char *content)
{
    conversation_msg_t *m = session_alloc(s);
    strncpy(m->role,    role,    sizeof(m->role)    - 1);
    strncpy(m->content, content, sizeof(m->content) - 1);
    m->is_tool_use    = false;
    m->is_tool_result = false;
}

void session_append_tool_use(session_t *s, const char *tool_id,
                              const char *tool_name, const char *input_json)
{
    conversation_msg_t *m = session_alloc(s);
    strncpy(m->role,     "assistant", sizeof(m->role)     - 1);
    strncpy(m->tool_id,  tool_id,     sizeof(m->tool_id)  - 1);
    strncpy(m->tool_name,tool_name,   sizeof(m->tool_name)- 1);
    strncpy(m->content,  input_json,  sizeof(m->content)  - 1);
    m->is_tool_use    = true;
    m->is_tool_result = false;
}

void session_append_tool_result(session_t *s, const char *tool_id,
                                 const char *result)
{
    conversation_msg_t *m = session_alloc(s);
    strncpy(m->role,    "user",   sizeof(m->role)    - 1);
    strncpy(m->tool_id, tool_id,  sizeof(m->tool_id) - 1);
    strncpy(m->content, result,   sizeof(m->content) - 1);
    m->is_tool_use    = false;
    m->is_tool_result = true;
}

/* -----------------------------------------------------------------------
 * JSON string escaping into out[out_sz], returns bytes written or -1
 * ----------------------------------------------------------------------- */
static int json_escape(const char *src, char *out, size_t out_sz)
{
    size_t pos = 0;
    for (; *src; src++) {
        unsigned char c = (unsigned char)*src;
        if (pos + 6 >= out_sz) return -1;  /* reserve room for worst-case \uXXXX */
        switch (c) {
        case '"':  out[pos++] = '\\'; out[pos++] = '"';  break;
        case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
        case '\n': out[pos++] = '\\'; out[pos++] = 'n';  break;
        case '\r': out[pos++] = '\\'; out[pos++] = 'r';  break;
        case '\t': out[pos++] = '\\'; out[pos++] = 't';  break;
        default:
            if (c < 0x20) {
                /* Control character: \uXXXX */
                pos += snprintf(out + pos, out_sz - pos, "\\u%04x", c);
            } else {
                out[pos++] = (char)c;
            }
        }
    }
    out[pos] = '\0';
    return (int)pos;
}

/* -----------------------------------------------------------------------
 * Append a single {"role":"...", "content":"..."} object
 * Returns bytes written, -1 on truncation
 * ----------------------------------------------------------------------- */
static int append_plain(char *out, size_t out_sz, size_t pos,
                         const char *role, const char *content, bool comma)
{
    /* Opening */
    int n = snprintf(out + pos, out_sz - pos,
                     "%s{\"role\":\"%s\",\"content\":\"", comma ? "," : "", role);
    if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
    pos += n;

    /* Escaped content */
    int esc = json_escape(content, out + pos, out_sz - pos - 3);
    if (esc < 0) return -1;
    pos += esc;

    /* Closing */
    if (pos + 3 >= out_sz) return -1;
    out[pos++] = '"';
    out[pos++] = '}';
    out[pos]   = '\0';
    return (int)pos;
}

int session_build_messages_json(const session_t *s, char *out, size_t out_sz)
{
    if (out_sz < 4) return -1;
    size_t pos = 0;
    out[pos++] = '[';

    for (int i = 0; i < s->count; i++) {
        int idx = (s->head + i) % MAX_HISTORY_TURNS;
        const conversation_msg_t *m = &s->turns[idx];
        bool comma = (i > 0);

        if (m->is_tool_use) {
            /*
             * Anthropic format: assistant message with content array,
             * type=tool_use block. content stores the input JSON.
             */
            int n = snprintf(out + pos, out_sz - pos,
                "%s{\"role\":\"assistant\",\"content\":[{\"type\":\"tool_use\","
                "\"id\":\"%s\",\"name\":\"%s\",\"input\":%s}]}",
                comma ? "," : "",
                m->tool_id, m->tool_name,
                m->content[0] ? m->content : "{}");
            if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
            pos += n;
        } else if (m->is_tool_result) {
            /* Anthropic format: role=user, content array with tool_result block */
            char esc_result[MAX_MESSAGE_LEN * 2];
            json_escape(m->content, esc_result, sizeof(esc_result));
            int n = snprintf(out + pos, out_sz - pos,
                "%s{\"role\":\"user\",\"content\":[{\"type\":\"tool_result\","
                "\"tool_use_id\":\"%s\",\"content\":\"%s\"}]}",
                comma ? "," : "",
                m->tool_id, esc_result);
            if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
            pos += n;
        } else {
            /* Plain user/assistant message */
            int newpos = append_plain(out, out_sz, pos,
                                      m->role, m->content, comma);
            if (newpos < 0) return -1;
            pos = (size_t)newpos;
        }
    }

    if (pos + 2 >= out_sz) return -1;
    out[pos++] = ']';
    out[pos]   = '\0';
    return (int)pos;
}

/* -----------------------------------------------------------------------
 * OpenAI-format messages: tool_calls + role=tool format
 * ----------------------------------------------------------------------- */
int session_build_messages_json_openai(const session_t *s, char *out, size_t out_sz)
{
    if (out_sz < 4) return -1;
    size_t pos = 0;
    out[pos++] = '[';

    for (int i = 0; i < s->count; i++) {
        int idx = (s->head + i) % MAX_HISTORY_TURNS;
        const conversation_msg_t *m = &s->turns[idx];
        bool comma = (i > 0);

        if (m->is_tool_use) {
            /* OpenAI format: assistant message with tool_calls array */
            char esc_args[MAX_MESSAGE_LEN * 2];
            json_escape(m->content[0] ? m->content : "{}", esc_args, sizeof(esc_args));
            int n = snprintf(out + pos, out_sz - pos,
                "%s{\"role\":\"assistant\",\"content\":null,"
                "\"tool_calls\":[{\"id\":\"%s\",\"type\":\"function\","
                "\"function\":{\"name\":\"%s\",\"arguments\":\"%s\"}}]}",
                comma ? "," : "",
                m->tool_id, m->tool_name, esc_args);
            if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
            pos += n;
        } else if (m->is_tool_result) {
            /* OpenAI format: role=tool, tool_call_id, content */
            char esc_result[MAX_MESSAGE_LEN * 2];
            json_escape(m->content, esc_result, sizeof(esc_result));
            int n = snprintf(out + pos, out_sz - pos,
                "%s{\"role\":\"tool\",\"tool_call_id\":\"%s\","
                "\"content\":\"%s\"}",
                comma ? "," : "",
                m->tool_id, esc_result);
            if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
            pos += n;
        } else {
            /* Plain user/assistant message */
            int newpos = append_plain(out, out_sz, pos,
                                      m->role, m->content, comma);
            if (newpos < 0) return -1;
            pos = (size_t)newpos;
        }
    }

    if (pos + 2 >= out_sz) return -1;
    out[pos++] = ']';
    out[pos]   = '\0';
    return (int)pos;
}
