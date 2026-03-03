/*
 * ESPClaw - agent/agent_loop.h
 * Agent task: reads inbound queue, calls LLM, writes outbound queue.
 */
#ifndef AGENT_LOOP_H
#define AGENT_LOOP_H

#include "bus/message_bus.h"
#include "esp_err.h"

/* Launch agent FreeRTOS task (pinned to ESPCLAW_CORE_AGENT on S3) */
esp_err_t agent_start(message_bus_t *bus);

#endif /* AGENT_LOOP_H */
