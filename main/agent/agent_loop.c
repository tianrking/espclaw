/*
 * ESPClaw - agent/agent_loop.c
 *
 * Step 6: ReAct Agent Loop — real tool dispatch
 *
 * Changes from Step 5:
 *   - Include tool_registry: build tools JSON, dispatch tool_use calls
 *   - Parse Anthropic tool_use block: extract name, tool_use_id, input
 *   - Pass tools_json to provider->complete()
 */
#include "agent_loop.h"
#include "session.h"
#include "context_builder.h"
#include "tool/tool_registry.h"
#include "provider/provider.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "platform.h"
#include "util/json_util.h"
#include "util/ratelimit.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG       = "agent";
static message_bus_t *s_bus = NULL;

/* Per-task state (one agent task only) */
static session_t s_session;

/* -----------------------------------------------------------------------
 * Parse Anthropic tool_use block and dispatch to tool_registry.
 *
 * Anthropic raw response (when stop_reason=tool_use):
 *   {"content":[...,{"type":"tool_use","id":"toolu_xxx","name":"gpio_write",
 *                    "input":{"pin":2,"state":1}}],"stop_reason":"tool_use"}
 *
 * Returns true if a tool_use was detected, dispatched, and result written.
 * ----------------------------------------------------------------------- */
static bool try_dispatch_tool(const char *reply,
                               char       *tool_id_out,  size_t tool_id_sz,
                               char       *tool_name_out,size_t tool_name_sz,
                               char       *input_out,    size_t input_sz,
                               char       *result_buf,   size_t result_sz)
{
    if (!strstr(reply, "\"stop_reason\":\"tool_use\"")) return false;

    /* Extract tool id */
    if (!json_get_str(reply, "id", tool_id_out, tool_id_sz)) {
        strncpy(tool_id_out, "unknown_id", tool_id_sz - 1);
    }

    /* Extract tool name */
    if (!json_get_str(reply, "name", tool_name_out, tool_name_sz)) {
        snprintf(result_buf, result_sz, "Error: could not parse tool name");
        return true; /* still a tool_use, just broken */
    }

    /* Extract input object (raw JSON) */
    const char *input_obj = json_get_object(reply, "input");
    const char *input_json = input_obj ? input_obj : "{}";

    /* Copy input JSON for caller to store in session */
    strncpy(input_out, input_json, input_sz - 1);
    input_out[input_sz - 1] = '\0';

    tool_registry_dispatch(tool_name_out, input_json, result_buf, result_sz);
    return true;
}

/* -----------------------------------------------------------------------
 * Agent task
 * ----------------------------------------------------------------------- */
static void agent_task(void *arg)
{
    inbound_msg_t  in;
    outbound_msg_t out;

    char *msgs_json   = ESPCLAW_MALLOC(LLM_REQUEST_BUF_SIZE);
    char *tools_json  = ESPCLAW_MALLOC(LLM_REQUEST_BUF_SIZE / 2);
    char *reply       = ESPCLAW_MALLOC(LLM_RESPONSE_BUF_SIZE);
    char *sys_prompt  = ESPCLAW_MALLOC(SYSTEM_PROMPT_BUF_SIZE);
    char *tool_id     = malloc(64);
    char *tool_name   = malloc(32);
    char *tool_input  = malloc(256);
    char *tool_result = malloc(TOOL_RESULT_BUF_SIZE);

    if (!msgs_json || !tools_json || !reply || !sys_prompt ||
        !tool_id   || !tool_name || !tool_input || !tool_result) {
        ESP_LOGE(TAG, "OOM at startup");
        goto done;
    }

    session_init(&s_session);
    tool_registry_init();

    /* Build tools JSON once (static table, doesn't change at runtime) */
    if (tool_registry_build_tools_json(tools_json, LLM_REQUEST_BUF_SIZE / 2) < 0) {
        ESP_LOGW(TAG, "tools JSON truncated");
        tools_json[0] = '\0';
    }

    ESP_LOGI(TAG, "ReAct agent ready (%d tools, history=%d, max_rounds=%d)",
             tool_registry_count(), MAX_HISTORY_TURNS, MAX_TOOL_ROUNDS);

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

        /* Rate limit check */
        char rl_reason[128];
        if (!ratelimit_check(rl_reason, sizeof(rl_reason))) {
            snprintf(out.text, sizeof(out.text), "[rate limited] %s", rl_reason);
            out.target  = in.source;
            out.chat_id = in.chat_id;
            message_bus_post_outbound(s_bus, &out, pdMS_TO_TICKS(200));
            continue;
        }

        /* 1. Add user turn */
        session_append(&s_session, "user", in.text);

        /* 2. Build system prompt */
        context_build_system_prompt(sys_prompt, SYSTEM_PROMPT_BUF_SIZE, NULL);

        /* 3. ReAct loop */
        bool got_reply = false;
        for (int round = 0; round < MAX_TOOL_ROUNDS; round++) {

            /* Choose message format based on provider type */
            bool use_openai_format = (llm->name &&
                (strcmp(llm->name, "openai") == 0 ||
                 strcmp(llm->name, "openrouter") == 0 ||
                 strcmp(llm->name, "ollama") == 0));

            int jlen = use_openai_format
                ? session_build_messages_json_openai(&s_session, msgs_json, LLM_REQUEST_BUF_SIZE)
                : session_build_messages_json(&s_session, msgs_json, LLM_REQUEST_BUF_SIZE);
            if (jlen < 0) ESP_LOGW(TAG, "messages JSON truncated");

            ESP_LOGI(TAG, "-> LLM round %d (%d chars)", round,
                     jlen > 0 ? jlen : 0);

            esp_err_t err = llm->complete(sys_prompt, msgs_json, tools_json,
                                          reply, LLM_RESPONSE_BUF_SIZE);
            if (err == ESP_OK) {
                ratelimit_record_request();
            }
            if (err != ESP_OK) {
                snprintf(out.text, sizeof(out.text),
                         "[error] LLM call failed: %s", esp_err_to_name(err));
                got_reply = true;
                break;
            }

            /* 4. Tool dispatch? */
            tool_id[0] = tool_name[0] = tool_input[0] = tool_result[0] = '\0';
            if (try_dispatch_tool(reply,
                                   tool_id,    64,
                                   tool_name,  32,
                                   tool_input, 256,
                                   tool_result, TOOL_RESULT_BUF_SIZE)) {
                session_append_tool_use(&s_session, tool_id, tool_name, tool_input);
                session_append_tool_result(&s_session, tool_id, tool_result);
                ESP_LOGI(TAG, "Tool: %s(%s) -> %s", tool_name, tool_input, tool_result);
                continue;
            }

            /* 5. Plain text reply — done */
            session_append(&s_session, "assistant", reply);
            strncpy(out.text, reply, sizeof(out.text) - 1);
            out.text[sizeof(out.text) - 1] = '\0';
            got_reply = true;
            break;
        }

        if (!got_reply) {
            snprintf(out.text, sizeof(out.text),
                     "[error] Max tool rounds (%d) exceeded.", MAX_TOOL_ROUNDS);
        }

        out.target  = in.source;
        out.chat_id = in.chat_id;
        message_bus_post_outbound(s_bus, &out, pdMS_TO_TICKS(200));
    }

done:
    ESPCLAW_FREE(msgs_json);
    ESPCLAW_FREE(tools_json);
    ESPCLAW_FREE(reply);
    ESPCLAW_FREE(sys_prompt);
    free(tool_id);
    free(tool_name);
    free(tool_input);
    free(tool_result);
    vTaskDelete(NULL);
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
