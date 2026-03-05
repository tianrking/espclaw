/*
 * ESPClaw - tool/tool_registry.c
 * Tool registration, JSON schema building, and dispatch.
 */
#include "tool_registry.h"
#include "tool.h"
#include "agent/persona.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "tool_reg";

/* ---- Pull in handler implementations first (before the table) ---- */
#include "tool_gpio.c"
#include "tool_memory.c"
#include "tool_system.c"
#include "tool_cron.c"
#include "tool_persona.c"

/*
 * X-macro: build static tool table.
 * Param names must NOT match any struct field name (avoid .name clash).
 */
#define TOOL_ENTRY(tname, tdesc, tschema, tfn) \
    { .name = (tname), .description = (tdesc), \
      .input_schema_json = (tschema), .execute = (tfn) },

static const tool_def_t s_tools[] = {
#include "builtin_tools.def"
};

#undef TOOL_ENTRY

static const int s_tool_count = (int)(sizeof(s_tools) / sizeof(s_tools[0]));

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void tool_registry_init(void)
{
    ESP_LOGI(TAG, "Registered %d tools:", s_tool_count);
    for (int i = 0; i < s_tool_count; i++)
        ESP_LOGI(TAG, "  %s", s_tools[i].name);
}

int tool_registry_count(void) { return s_tool_count; }

int tool_registry_build_tools_json(char *out, size_t out_sz)
{
    size_t pos = 0;
    out[pos++] = '[';
    for (int i = 0; i < s_tool_count; i++) {
        /* Anthropic tool format: {name, description, input_schema} */
        int n = snprintf(out + pos, out_sz - pos,
            "%s{\"name\":\"%s\",\"description\":\"%s\","
            "\"input_schema\":%s}",
            i > 0 ? "," : "",
            s_tools[i].name,
            s_tools[i].description,
            s_tools[i].input_schema_json);
        if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
        pos += n;
    }
    if (pos + 2 >= out_sz) return -1;
    out[pos++] = ']';
    out[pos]   = '\0';
    return (int)pos;
}

int tool_registry_build_tools_json_openai(char *out, size_t out_sz)
{
    size_t pos = 0;
    out[pos++] = '[';
    for (int i = 0; i < s_tool_count; i++) {
        /* OpenAI / GLM-4 format: {type:function, function:{name,description,parameters}} */
        int n = snprintf(out + pos, out_sz - pos,
            "%s{\"type\":\"function\",\"function\":{"
            "\"name\":\"%s\",\"description\":\"%s\","
            "\"parameters\":%s}}",
            i > 0 ? "," : "",
            s_tools[i].name,
            s_tools[i].description,
            s_tools[i].input_schema_json);
        if (n <= 0 || (size_t)n >= out_sz - pos) return -1;
        pos += n;
    }
    if (pos + 2 >= out_sz) return -1;
    out[pos++] = ']';
    out[pos]   = '\0';
    return (int)pos;
}

bool tool_registry_dispatch(const char *name, const char *input_json,
                             char *result_buf, size_t result_sz)
{
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Exec: %s", name);
            return s_tools[i].execute(input_json ? input_json : "{}",
                                      result_buf, result_sz);
        }
    }
    snprintf(result_buf, result_sz, "Unknown tool: %s", name);
    return false;
}
