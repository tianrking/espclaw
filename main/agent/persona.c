/*
 * ESPClaw - agent/persona.c
 * Persona (personality) system implementation.
 */
#include "persona.h"
#include "config.h"
#include "nvs_keys.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "persona";

static persona_type_t s_persona = PERSONA_NEUTRAL;

/* Persona names */
static const char *s_persona_names[] = {
    [PERSONA_NEUTRAL]   = "neutral",
    [PERSONA_FRIENDLY]  = "friendly",
    [PERSONA_TECHNICAL] = "technical",
    [PERSONA_WITTY]     = "witty",
};

/* Persona instructions (injected into system prompt) */
static const char *s_persona_instructions[] = {
    [PERSONA_NEUTRAL]   = "Use direct, plain wording.",
    [PERSONA_FRIENDLY]  = "Use warm, approachable wording while staying concise.",
    [PERSONA_TECHNICAL] = "Use precise technical language and concrete terminology.",
    [PERSONA_WITTY]     = "Use a lightly witty tone; at most one brief witty flourish per reply.",
};

/* All persona names as comma-separated string */
static const char *s_persona_list = "neutral, friendly, technical, witty";

void persona_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No NVS persona stored, using default: neutral");
        s_persona = PERSONA_NEUTRAL;
        return;
    }

    char stored[32] = {0};
    err = nvs_get_str(h, NVS_KEY_PERSONA, stored, &(size_t){sizeof(stored)});
    nvs_close(h);

    if (err != ESP_OK) {
        s_persona = PERSONA_NEUTRAL;
        ESP_LOGI(TAG, "No persona in NVS, using default: neutral");
        return;
    }

    /* Lowercase for comparison */
    for (size_t i = 0; stored[i]; i++) {
        stored[i] = (char)tolower((unsigned char)stored[i]);
    }

    persona_type_t parsed;
    if (!persona_parse(stored, &parsed)) {
        ESP_LOGW(TAG, "Invalid stored persona '%s', using neutral", stored);
        s_persona = PERSONA_NEUTRAL;
        return;
    }

    s_persona = parsed;
    ESP_LOGI(TAG, "Loaded persona from NVS: %s", persona_name(s_persona));
}

persona_type_t persona_get(void)
{
    return s_persona;
}

const char *persona_name(persona_type_t p)
{
    if (p < 0 || p >= PERSONA_COUNT) {
        return "neutral";
    }
    return s_persona_names[p];
}

const char *persona_instruction(persona_type_t p)
{
    if (p < 0 || p >= PERSONA_COUNT) {
        return s_persona_instructions[PERSONA_NEUTRAL];
    }
    return s_persona_instructions[p];
}

bool persona_set(persona_type_t p)
{
    if (p < 0 || p >= PERSONA_COUNT) {
        ESP_LOGW(TAG, "Invalid persona: %d", p);
        return false;
    }

    s_persona = p;

    /* Save to NVS */
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for persona save: %s", esp_err_to_name(err));
        return true; /* Still accept the change, just not persisted */
    }

    err = nvs_set_str(h, NVS_KEY_PERSONA, s_persona_names[p]);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save persona: %s", esp_err_to_name(err));
    } else {
        nvs_commit(h);
        ESP_LOGI(TAG, "Saved persona: %s", s_persona_names[p]);
    }
    nvs_close(h);

    return true;
}

bool persona_parse(const char *name, persona_type_t *out)
{
    if (!name || !out) return false;

    char lower[32];
    strncpy(lower, name, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (size_t i = 0; lower[i]; i++) {
        lower[i] = (char)tolower((unsigned char)lower[i]);
    }

    for (int i = 0; i < PERSONA_COUNT; i++) {
        if (strcmp(lower, s_persona_names[i]) == 0) {
            *out = (persona_type_t)i;
            return true;
        }
    }
    return false;
}

const char *persona_list(void)
{
    return s_persona_list;
}
