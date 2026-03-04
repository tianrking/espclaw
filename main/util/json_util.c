/*
 * ESPClaw - util/json_util.c
 * Lightweight JSON field extraction (no cJSON dependency on C3).
 */
#include "json_util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Find "key": in json, return pointer past the colon+whitespace */
static const char *find_value(const char *json, const char *key)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, needle)) != NULL) {
        p += strlen(needle);
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ':') { p++; while (*p == ' ' || *p == '\t') p++; return p; }
    }
    return NULL;
}

bool json_get_str(const char *json, const char *key, char *out, size_t out_sz)
{
    const char *v = find_value(json, key);
    if (!v || *v != '"') return false;
    v++; /* skip opening quote */
    size_t pos = 0;
    while (*v && pos < out_sz - 1) {
        if (v[0] == '\\' && v[1]) {
            switch (v[1]) {
            case '"':  out[pos++] = '"';  break;
            case '\\': out[pos++] = '\\'; break;
            case 'n':  out[pos++] = '\n'; break;
            case 'r':  out[pos++] = '\r'; break;
            case 't':  out[pos++] = '\t'; break;
            default:   out[pos++] = v[1]; break;
            }
            v += 2;
        } else if (v[0] == '"') {
            break;
        } else {
            out[pos++] = *v++;
        }
    }
    out[pos] = '\0';
    return true;
}

bool json_get_int(const char *json, const char *key, int *out)
{
    const char *v = find_value(json, key);
    if (!v) return false;
    char *end;
    long val = strtol(v, &end, 10);
    if (end == v) return false;
    *out = (int)val;
    return true;
}

const char *json_get_object(const char *json, const char *key)
{
    const char *v = find_value(json, key);
    if (!v) return NULL;
    if (*v == '{' || *v == '[') return v;
    return NULL;
}

void json_unescape(char *buf)
{
    char *r = buf, *w = buf;
    while (*r) {
        if (r[0] == '\\' && r[1]) {
            switch (r[1]) {
            case '"':  *w++ = '"';  r += 2; break;
            case '\\': *w++ = '\\'; r += 2; break;
            case 'n':  *w++ = '\n'; r += 2; break;
            case 'r':  *w++ = '\r'; r += 2; break;
            case 't':  *w++ = '\t'; r += 2; break;
            default:   *w++ = r[1]; r += 2; break;
            }
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}
