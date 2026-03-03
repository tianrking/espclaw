/*
 * ESPClaw - util/text_buffer.c
 */
#include "text_buffer.h"
#include <string.h>
#include <stdio.h>

void text_buffer_init(text_buffer_t *tb, char *buf, size_t capacity)
{
    tb->buf = buf;
    tb->capacity = capacity;
    tb->len = 0;
    tb->overflow = false;
    if (capacity > 0) buf[0] = '\0';
}

bool text_buffer_append(text_buffer_t *tb, const char *str)
{
    if (tb->overflow || !str) return false;
    size_t slen = strlen(str);
    if (tb->len + slen >= tb->capacity) {
        tb->overflow = true;
        return false;
    }
    memcpy(tb->buf + tb->len, str, slen);
    tb->len += slen;
    tb->buf[tb->len] = '\0';
    return true;
}

bool text_buffer_append_fmt(text_buffer_t *tb, const char *fmt, ...)
{
    if (tb->overflow) return false;
    size_t remaining = tb->capacity - tb->len;
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(tb->buf + tb->len, remaining, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= remaining) {
        tb->overflow = true;
        return false;
    }
    tb->len += (size_t)written;
    return true;
}

void text_buffer_reset(text_buffer_t *tb)
{
    tb->len = 0;
    tb->overflow = false;
    if (tb->capacity > 0) tb->buf[0] = '\0';
}

const char *text_buffer_str(const text_buffer_t *tb)
{
    return tb->buf;
}

size_t text_buffer_len(const text_buffer_t *tb)
{
    return tb->len;
}
