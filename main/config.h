/*
 * ESPClaw - config.h
 * All compile-time constants. Platform-conditional sizing for C3 vs S3.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "platform.h"

/* -----------------------------------------------------------------------
 * Firmware Version
 * ----------------------------------------------------------------------- */
#define ESPCLAW_VERSION         "1.0.0"

/* -----------------------------------------------------------------------
 * Buffer Sizes (platform-dependent)
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_PSRAM
  /* S3: generous buffers allocated from PSRAM */
  #define LLM_REQUEST_BUF_SIZE     32768   /* 32KB */
  #define LLM_RESPONSE_BUF_SIZE    32768   /* 32KB */
  #define CHANNEL_RX_BUF_SIZE      1024
  #define CHANNEL_TX_BUF_SIZE      2048
  #define TOOL_RESULT_BUF_SIZE     1024
  #define MAX_MESSAGE_LEN          2048
  #define SYSTEM_PROMPT_BUF_SIZE   4096
  #define JSON_PARSE_BUF_SIZE      32768   /* 32KB for cJSON on PSRAM */
  #define SESSION_HISTORY_SIZE     32768   /* 32KB session .jsonl */
#else
  /* C3: tight buffers, all from internal SRAM */
  #define LLM_REQUEST_BUF_SIZE     8192    /* 8KB — actual request ~6.5KB */
  #define LLM_RESPONSE_BUF_SIZE    8192    /* 8KB — embedded responses are concise */
  #define CHANNEL_RX_BUF_SIZE      512
  #define CHANNEL_TX_BUF_SIZE      1024
  #define TOOL_RESULT_BUF_SIZE     512
  #define MAX_MESSAGE_LEN          1024
  #define SYSTEM_PROMPT_BUF_SIZE   2048
  #define JSON_PARSE_BUF_SIZE      0       /* Use streaming parser */
  #define SESSION_HISTORY_SIZE     0       /* No session persistence */
#endif

/* -----------------------------------------------------------------------
 * Conversation History
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_PSRAM
  #define MAX_HISTORY_TURNS        24
#else
  #define MAX_HISTORY_TURNS        8
#endif

/* -----------------------------------------------------------------------
 * Agent Loop
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_PSRAM
  #define MAX_TOOL_ROUNDS          10
#else
  #define MAX_TOOL_ROUNDS          5
#endif

/* -----------------------------------------------------------------------
 * FreeRTOS Tasks
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_PSRAM
  #define AGENT_TASK_STACK_SIZE    12288
  #define CHANNEL_TASK_STACK_SIZE  8192
  #define TG_POLL_TASK_STACK_SIZE  12288
  #define TG_OUTPUT_TASK_STACK_SIZE 12288
  #define OUTBOUND_TASK_STACK_SIZE 8192
  #define CRON_TASK_STACK_SIZE     8192
  #define BOOT_OK_TASK_STACK_SIZE  4096
  #define WS_TASK_STACK_SIZE       8192
  #define HEARTBEAT_TASK_STACK_SIZE 8192
#else
  #define AGENT_TASK_STACK_SIZE    8192
  #define CHANNEL_TASK_STACK_SIZE  4096
  #define TG_POLL_TASK_STACK_SIZE  8192
  #define TG_OUTPUT_TASK_STACK_SIZE 8192  /* Needs extra space for 4KB msg buffer */
  #define OUTBOUND_TASK_STACK_SIZE 4096
  #define CRON_TASK_STACK_SIZE     4096
  #define BOOT_OK_TASK_STACK_SIZE  4096
  #define WS_TASK_STACK_SIZE       0       /* Not used on C3 */
  #define HEARTBEAT_TASK_STACK_SIZE 0      /* Not used on C3 */
#endif

#define AGENT_TASK_PRIORITY        5
#define CHANNEL_TASK_PRIORITY      5
#define TG_POLL_TASK_PRIORITY      5
#define OUTBOUND_TASK_PRIORITY     5
#define CRON_TASK_PRIORITY         4
#define WS_TASK_PRIORITY           4
#define HEARTBEAT_TASK_PRIORITY    3

/* -----------------------------------------------------------------------
 * Queues
 * ----------------------------------------------------------------------- */
#define INPUT_QUEUE_LENGTH         8
#define OUTPUT_QUEUE_LENGTH        8
#define TELEGRAM_OUTPUT_QUEUE_LENGTH 2

/* -----------------------------------------------------------------------
 * LLM Backend Configuration
 * ----------------------------------------------------------------------- */
typedef enum {
    LLM_BACKEND_ANTHROPIC   = 0,
    LLM_BACKEND_OPENAI      = 1,
    LLM_BACKEND_OPENROUTER  = 2,
    LLM_BACKEND_OLLAMA      = 3,
    LLM_BACKEND_CUSTOM      = 4,  /* any OpenAI-compatible endpoint */
} llm_backend_t;

#define LLM_API_URL_ANTHROPIC    "https://api.anthropic.com/v1/messages"
#define LLM_API_URL_OPENAI       "https://api.openai.com/v1/chat/completions"
#define LLM_API_URL_OPENROUTER   "https://openrouter.ai/api/v1/chat/completions"
#define LLM_API_URL_OLLAMA       "http://127.0.0.1:11434/v1/chat/completions"

#define LLM_DEFAULT_MODEL_ANTHROPIC  "claude-haiku-4-5-20251001"
#define LLM_DEFAULT_MODEL_OPENAI     "gpt-4o-mini"
#define LLM_DEFAULT_MODEL_OPENROUTER "anthropic/claude-haiku"
#define LLM_DEFAULT_MODEL_OLLAMA     "qwen2.5:7b"

#define LLM_API_KEY_MAX_LEN      511
#define LLM_API_KEY_BUF_SIZE     (LLM_API_KEY_MAX_LEN + 1)
#define LLM_MAX_TOKENS           1024
#define LLM_HTTP_TIMEOUT_MS      60000   /* 60s for slow LLM APIs */
#define LLM_MAX_RETRIES          3
#define LLM_RETRY_BASE_MS        2000
#define LLM_RETRY_MAX_MS         10000
#define LLM_RETRY_BUDGET_MS      45000

/* -----------------------------------------------------------------------
 * GPIO Safety (Kconfig-driven)
 * ----------------------------------------------------------------------- */
#ifdef CONFIG_ESPCLAW_GPIO_MIN_PIN
  #define GPIO_MIN_PIN            CONFIG_ESPCLAW_GPIO_MIN_PIN
#else
  #define GPIO_MIN_PIN            2
#endif
#ifdef CONFIG_ESPCLAW_GPIO_MAX_PIN
  #define GPIO_MAX_PIN            CONFIG_ESPCLAW_GPIO_MAX_PIN
#else
  #define GPIO_MAX_PIN            10
#endif
#ifdef CONFIG_ESPCLAW_GPIO_ALLOWED_PINS
  #define GPIO_ALLOWED_PINS_CSV   CONFIG_ESPCLAW_GPIO_ALLOWED_PINS
#else
  #define GPIO_ALLOWED_PINS_CSV   ""
#endif

/* -----------------------------------------------------------------------
 * NVS
 * ----------------------------------------------------------------------- */
#define NVS_NAMESPACE            "espclaw"
#define NVS_NAMESPACE_CRON       "ec_cron"
#define NVS_NAMESPACE_TOOLS      "ec_tools"
#define NVS_NAMESPACE_CONFIG     "ec_config"
#define NVS_MAX_KEY_LEN          15
#define NVS_MAX_VALUE_LEN        512

/* -----------------------------------------------------------------------
 * WiFi
 * ----------------------------------------------------------------------- */
#define WIFI_MAX_RETRY           10
#define WIFI_RETRY_DELAY_MS      1000

/* -----------------------------------------------------------------------
 * Telegram
 * ----------------------------------------------------------------------- */
#define TELEGRAM_API_URL         "https://api.telegram.org/bot"
#define TELEGRAM_POLL_TIMEOUT    5       /* Short poll to free heap quickly */
#define TELEGRAM_POLL_TIMEOUT_OPENROUTER 5
#define TELEGRAM_POLL_INTERVAL   500     /* ms between poll attempts */
#define TELEGRAM_MAX_MSG_LEN     4096
#define TELEGRAM_FLUSH_ON_START  1
#define START_COMMAND_COOLDOWN_MS 30000
#define TELEGRAM_MAX_ALLOWED_CHAT_IDS 4
#define TELEGRAM_HTTP_TIMEOUT_MS 10000   /* Poll timeout + margin */
/* Exponential backoff */
#define TELEGRAM_BACKOFF_BASE_MS      5000    /* 5 seconds */
#define TELEGRAM_BACKOFF_MAX_MS      300000    /* 5 minutes */
#define TELEGRAM_BACKOFF_MULTIPLIER  2
/* Stale poll detection and auto-resync */
#define TELEGRAM_STALE_POLL_LOG_INTERVAL 4       /* Log every N stale-only polls */
#define TELEGRAM_STALE_POLL_RESYNC_STREAK 8      /* Trigger auto-resync after this streak */
#define TELEGRAM_STALE_POLL_RESYNC_COOLDOWN_MS 60000 /* Min gap between auto-resync attempts */

/* -----------------------------------------------------------------------
 * WebSocket (S3 only)
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_WEBSOCKET
  #define WS_PORT                18789
  #define WS_MAX_CLIENTS         4
#endif

/* -----------------------------------------------------------------------
 * Cron
 * ----------------------------------------------------------------------- */
#define CRON_CHECK_INTERVAL_MS   10000
#define CRON_MAX_ENTRIES         10
#define CRON_MAX_ACTION_LEN      256
#define CRON_TASK_STACK_SIZE     4096
#define CRON_TASK_PRIORITY       4
#define NTP_SYNC_TIMEOUT_MS      10000
#define DEFAULT_TIMEZONE_POSIX   "UTC0"
#define TIMEZONE_MAX_LEN         64

/* -----------------------------------------------------------------------
 * Factory Reset
 * ----------------------------------------------------------------------- */
#ifdef CONFIG_ESPCLAW_FACTORY_RESET_PIN
  #define FACTORY_RESET_PIN      CONFIG_ESPCLAW_FACTORY_RESET_PIN
#else
  #define FACTORY_RESET_PIN      9
#endif
#define FACTORY_RESET_HOLD_MS    5000

/* -----------------------------------------------------------------------
 * Rate Limiting
 * ----------------------------------------------------------------------- */
#define RATELIMIT_MAX_PER_HOUR   100
#define RATELIMIT_MAX_PER_DAY    1000
#define RATELIMIT_ENABLED        1

/* -----------------------------------------------------------------------
 * Boot Guard
 * ----------------------------------------------------------------------- */
#define MAX_BOOT_FAILURES        4
#define BOOT_SUCCESS_DELAY_MS    30000

/* -----------------------------------------------------------------------
 * NTP
 * ----------------------------------------------------------------------- */
#define NTP_SERVER               "pool.ntp.org"

/* -----------------------------------------------------------------------
 * Heartbeat (S3 only)
 * ----------------------------------------------------------------------- */
#if ESPCLAW_HAS_HEARTBEAT
  #define HEARTBEAT_INTERVAL_MS  1800000  /* 30 minutes */
#endif

/* -----------------------------------------------------------------------
 * Dynamic Tools
 * ----------------------------------------------------------------------- */
#define MAX_DYNAMIC_TOOLS        8
#define TOOL_NAME_MAX_LEN        24
#define TOOL_DESC_MAX_LEN        128

#endif /* CONFIG_H */
