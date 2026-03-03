/*
 * ESPClaw - agent/agent_loop.c
 *
 * Step 4: single-turn LLM chat, no tool dispatch yet.
 *
 * Flow:
 *   inbound_msg_t  →  build messages JSON  →  provider->complete()
 *                  →  outbound_msg_t back to same channel
 *
 * History: last MAX_HISTORY_TURNS turns kept in a circular ring.
 */
#include "agent_loop.h"
#include "provider/provider.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "platform.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG        = "agent";
static message_bus_t *s_bus  = NULL;

/* Conversation history ring buffer */
static conversation_msg_t s_history[MAX_HISTORY_TURNS];
static int s_history_count = 0;
static int s_history_head  = 0;  /* oldest entry index */

static void history_append(const char *role, const char *content)
{
    int idx;
    if (s_history_count < MAX_HISTORY_TURNS) {
        idx = s_history_count++;
    } else {
        /* Overwrite oldest */
        idx = s_history_head;
        s_history_head = (s_history_head + 1) % MAX_HISTORY_TURNS;
    }
    strncpy(s_history[idx].role,    role,    sizeof(s_history[idx].role)    - 1);
    strncpy(s_history[idx].content, content, sizeof(s_history[idx].content) - 1);
    s_history[idx].role[sizeof(s_history[idx].role) - 1]       = '\0';
    s_history[idx].content[sizeof(s_history[idx].content) - 1] = '\0';
}

/* Build JSON array: [{"role":"user","content":"..."},{"role":"assistant",...}] */
static void build_messages_json(char *out, size_t out_sz)
{
    size_t pos = 0;
    out[pos++] = '[';

    for (int i = 0; i < s_history_count; i++) {
        int idx = (s_history_head + i) % MAX_HISTORY_TURNS;
        const conversation_msg_t *m = &s_history[idx];

        /* Estimate worst-case: role(16) + content escaped (~2x) + overhead */
        int written = snprintf(out + pos, out_sz - pos - 2,
            "%s{\"role\":\"%s\",\"content\":\"",
            i > 0 ? "," : "", m->role);
        if (written <= 0) break;
        pos += written;

        /* Escape content */
        for (const char *c = m->content; *c && pos < out_sz - 4; c++) {
            if (*c == '"')       { out[pos++] = '\\'; out[pos++] = '"'; }
            else if (*c == '\\') { out[pos++] = '\\'; out[pos++] = '\\'; }
            else if (*c == '\n') { out[pos++] = '\\'; out[pos++] = 'n'; }
            else                 { out[pos++] = *c; }
        }

        if (pos + 3 < out_sz) {
            out[pos++] = '"';
            out[pos++] = '}';
        }
    }

    if (pos < out_sz) out[pos++] = ']';
    out[pos] = '\0';
}

static void agent_task(void *arg)
{
    inbound_msg_t  in;
    outbound_msg_t out;

    char *msgs_json  = malloc(LLM_REQUEST_BUF_SIZE);
    char *reply      = malloc(CHANNEL_TX_BUF_SIZE);

    if (!msgs_json || !reply) {
        ESP_LOGE(TAG, "OOM at startup");
        free(msgs_json); free(reply);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Agent ready (history=%d turns)", MAX_HISTORY_TURNS);

    while (1) {
        if (xQueueReceive(s_bus->inbound, &in, portMAX_DELAY) != pdTRUE)
            continue;

        const provider_ops_t *llm = provider_get_active();
        if (!llm) {
            snprintf(out.text, sizeof(out.text),
                     "[error] No LLM configured. Set API key via menuconfig.");
            out.target  = in.source;
            out.chat_id = in.chat_id;
            message_bus_post_outbound(s_bus, &out, pdMS_TO_TICKS(200));
            continue;
        }

        /* Add user turn to history */
        history_append("user", in.text);

        /* Build messages JSON */
        build_messages_json(msgs_json, LLM_REQUEST_BUF_SIZE);

        /* Call LLM */
        ESP_LOGI(TAG, "→ LLM (%d chars)", (int)strlen(msgs_json));
        esp_err_t err = llm->complete(
            "You are ESPClaw, an embedded AI assistant by Seeed Studio running on an ESP32 microcontroller. You are concise, technical, and helpful.",
            msgs_json, NULL,
            reply, CHANNEL_TX_BUF_SIZE);

        if (err != ESP_OK) {
            snprintf(out.text, sizeof(out.text), "[error] LLM call failed: %s",
                     esp_err_to_name(err));
        } else {
            /* Add assistant turn to history */
            history_append("assistant", reply);
            strncpy(out.text, reply, sizeof(out.text) - 1);
            out.text[sizeof(out.text) - 1] = '\0';
        }

        out.target  = in.source;
        out.chat_id = in.chat_id;
        message_bus_post_outbound(s_bus, &out, pdMS_TO_TICKS(200));
    }

    /* unreachable */
    free(msgs_json);
    free(reply);
}

esp_err_t agent_start(message_bus_t *bus)
{
    s_bus = bus;

    BaseType_t ret = ESPCLAW_CREATE_PINNED(
        "agent", agent_task, AGENT_TASK_STACK_SIZE,
        NULL, AGENT_TASK_PRIORITY, NULL, ESPCLAW_CORE_AGENT);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create agent task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
