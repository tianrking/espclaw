/*
 * ESPClaw - agent/persona.h
 * Persona (personality) system for controlling LLM response style.
 */
#ifndef PERSONA_H
#define PERSONA_H

#include <stddef.h>
#include <stdbool.h>

/* Persona types - affect wording style only */
typedef enum {
    PERSONA_NEUTRAL = 0,    /* Direct, plain wording (default) */
    PERSONA_FRIENDLY,       /* Warm, approachable */
    PERSONA_TECHNICAL,      /* Precise technical language */
    PERSONA_WITTY,          /* Lightly humorous */
    PERSONA_COUNT
} persona_type_t;

/* Initialize persona system (load from NVS) */
void persona_init(void);

/* Get current persona */
persona_type_t persona_get(void);

/* Get persona name string */
const char *persona_name(persona_type_t p);

/* Get persona instruction for system prompt */
const char *persona_instruction(persona_type_t p);

/* Set persona (saves to NVS, returns false if invalid) */
bool persona_set(persona_type_t p);

/* Parse persona name to enum (returns false if not found) */
bool persona_parse(const char *name, persona_type_t *out);

/* Get all persona names as comma-separated string */
const char *persona_list(void);

#endif /* PERSONA_H */
