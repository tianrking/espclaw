/*
 * ESPClaw - messages.h
 * Queue message type definitions for inter-task communication.
 */
#ifndef MESSAGES_H
#define MESSAGES_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

/* Message source / target channel */
typedef enum {
    MSG_SOURCE_SERIAL    = 0,
    MSG_SOURCE_TELEGRAM  = 1,
    MSG_SOURCE_CRON      = 2,
    MSG_SOURCE_WEBSOCKET = 3,   /* S3 only */
    MSG_SOURCE_HEARTBEAT = 4,   /* S3 only */
    MSG_SOURCE_DINGTALK  = 5,   /* One-way notification */
    MSG_SOURCE_DISCORD   = 6,   /* One-way notification */
    MSG_SOURCE_SLACK     = 7,   /* One-way notification */
    MSG_SOURCE_WECOM     = 8,   /* One-way notification */
    MSG_SOURCE_LARK      = 9,   /* One-way notification */
    MSG_SOURCE_PUSHPLUS  = 10,  /* One-way notification */
    MSG_SOURCE_BARK      = 11,  /* One-way notification */
} message_source_t;

/* Inbound message (any channel -> agent) */
typedef struct {
    char text[CHANNEL_RX_BUF_SIZE];
    message_source_t source;
    int64_t chat_id;            /* Telegram chat ID or WS client ID */
} inbound_msg_t;

/* Outbound message (agent -> any channel) */
typedef struct {
    char text[CHANNEL_TX_BUF_SIZE];
    message_source_t target;    /* Which channel to reply to */
    int64_t chat_id;
} outbound_msg_t;

/* Telegram-specific outbound (larger buffer for Telegram API limits) */
typedef struct {
    char text[TELEGRAM_MAX_MSG_LEN];
    int64_t chat_id;
} telegram_msg_t;

/* Conversation message (for history tracking in agent loop) */
typedef struct {
    char role[16];              /* "user", "assistant" */
    char content[MAX_MESSAGE_LEN];
    bool is_tool_use;
    bool is_tool_result;
    char tool_id[64];
    char tool_name[32];
} conversation_msg_t;

#endif /* MESSAGES_H */
