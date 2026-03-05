/*
 * ESPClaw - util/json_util.h
 * Lightweight JSON field extraction without cJSON (saves ~20KB on C3).
 * Step 6: used by tool registry to extract tool_use fields from LLM response.
 */
#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Extract a string field value from JSON object (first occurrence).
 * e.g. json_get_str("{\"name\":\"gpio_write\"}", "name", buf, sizeof(buf))
 * Returns true on success. Output is NUL-terminated and JSON-unescaped.
 */
bool json_get_str(const char *json, const char *key, char *out, size_t out_sz);

/*
 * Extract an integer field value from JSON object.
 * Returns true on success.
 */
bool json_get_int(const char *json, const char *key, int *out);

/*
 * Find the start of a JSON object or array for the given key.
 * Returns pointer into json, or NULL if not found.
 * e.g. key="input" would return pointer to "{\"pin\":2,\"state\":1}"
 */
const char *json_get_object(const char *json, const char *key);

/*
 * Copy a complete JSON object/array from src into dst.
 * Properly handles nested structures and escaped strings.
 * Returns characters copied (excl. NUL), or -1 on error.
 * Use this instead of json_get_object when you need a standalone copy.
 */
int json_copy_object(const char *src, char *dst, size_t dst_sz);

/*
 * Unescape a JSON string in-place (modifies buf).
 * Converts \" → ", \n → newline, \\ → \, etc.
 */
void json_unescape(char *buf);

#endif /* JSON_UTIL_H */
