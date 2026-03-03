/*
 * ESPClaw - util/text_buffer.h
 * Fixed-capacity text buffer with overflow detection.
 */
#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct {
    char *buf;
    size_t capacity;
    size_t len;
    bool overflow;
} text_buffer_t;

void text_buffer_init(text_buffer_t *tb, char *buf, size_t capacity);
bool text_buffer_append(text_buffer_t *tb, const char *str);
bool text_buffer_append_fmt(text_buffer_t *tb, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void text_buffer_reset(text_buffer_t *tb);
const char *text_buffer_str(const text_buffer_t *tb);
size_t text_buffer_len(const text_buffer_t *tb);

#endif /* TEXT_BUFFER_H */
