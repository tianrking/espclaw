/*
 * ESPClaw - tool/tool_persona.c
 * Persona tool handlers. Included directly by tool_registry.c (not a standalone TU).
 */
#include "agent/persona.h"
#include "util/json_util.h"
#include <stdio.h>

static bool set_persona_exec(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));
static bool get_persona_exec(const char *input_json, char *result_buf, size_t result_sz)
__attribute__((unused));

static bool set_persona_exec(const char *input_json, char *result_buf, size_t result_sz)
{
    char persona_name_buf[32] = {0};
    if (!json_get_str(input_json, "persona", persona_name_buf, sizeof(persona_name_buf))) {
        snprintf(result_buf, result_sz, "Error: 'persona' must be a string. Available: %s", persona_list());
        return false;
    }

    persona_type_t p;
    if (!persona_parse(persona_name_buf, &p)) {
        snprintf(result_buf, result_sz, "Error: Unknown persona '%s'. Available: %s", 
                 persona_name_buf, persona_list());
        return false;
    }

    persona_set(p);
    snprintf(result_buf, result_sz, "Persona changed to '%s'. %s", 
             persona_name(p), persona_instruction(p));
    return true;
}

static bool get_persona_exec(const char *input_json, char *result_buf, size_t result_sz)
{
    (void)input_json;
    persona_type_t p = persona_get();
    snprintf(result_buf, result_sz, "Current persona: '%s'. %s", 
             persona_name(p), persona_instruction(p));
    return true;
}
