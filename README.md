# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![Target](https://img.shields.io/badge/Target-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()

[🇨🇳 中文文档](README.zh.md)

ESP-IDF 5.5 AI assistant firmware for ESP32-C3 / C5 / S3.

## Demo

[Uploading esp32c5.mp4…](https://github.com/user-attachments/assets/4ca20a3d-8336-4232-ae38-6b1f12aa64bc)

### Persistent Memory

ESPClaw remembers user preferences across reboots via NVS storage:

<img src="media/memory.png" width="80%" alt="Memory demo - LLM remembers user name">

## Supported Targets

| Target | Cores | PSRAM | Flash | Profile |
|--------|-------|-------|-------|---------|
| ESP32-C3 | 1 | No | 4MB | MINIMAL |
| ESP32-C5 | 1 | No | 4MB | MINIMAL |
| ESP32-S3 | 2 | 8MB | 16MB | FULL |

## Build

```bash
# First time or after switching target — pick ONE:
idf.py set-target esp32c5
idf.py set-target esp32c3
idf.py set-target esp32s3

idf.py build flash monitor
```

## Switching Targets

> **Important:** When changing the target chip, you must delete `sdkconfig` first,
> otherwise the cached flash size and partition settings from the old target will
> cause a build error.

```bash
# Example: switch from C3 to C5
idf.py set-target esp32c5
rm sdkconfig          # ← required after every target switch
idf.py build
```

Why: `idf.py set-target` updates `sdkconfig.defaults.*` selection, but the local
`sdkconfig` file takes precedence and retains the old chip's defaults (e.g. 2MB flash
on C5 vs the 4MB we need). Deleting it forces a clean re-generation from
`sdkconfig.defaults` + `sdkconfig.defaults.<target>`.

## WiFi Notes

**WPA version compatibility (ESP32-C5):** If the device fails to connect, check the
security mode of your router. ESP32-C5's WiFi driver defaults to requiring WPA2 or
higher when a password is provided. Routers set to "WPA only" (not WPA2) will cause
authentication failures. Switch your router's security mode to **WPA2-PSK** or
**WPA2/WPA3 mixed mode**.

The driver prints the connected security type on success:
```
wifi: security: WPA2-PSK, phy:bgn, rssi:-41
```

## LLM Configuration

ESPClaw supports **any OpenAI-compatible API endpoint** (third-party providers,
self-hosted models, etc.) in addition to official Anthropic and OpenAI APIs.

### Quick setup via menuconfig (development)

```bash
idf.py menuconfig
# → ESPClaw Configuration → LLM Settings
#   Backend:  0=Anthropic  1=OpenAI  2=OpenRouter  3=Ollama  4=Custom
#   API Key:  your key
#   Base URL: https://your-provider.com/v1  (leave empty for official endpoints)
#   Model:    model name (e.g. gpt-4o-mini, claude-3-haiku, gemini-pro)
```

### Supported backends

| Backend | Wire format | Default endpoint |
|---------|-------------|-----------------|
| Anthropic | Messages API | `api.anthropic.com/v1/messages` |
| OpenAI | Chat Completions | `api.openai.com/v1/chat/completions` |
| OpenRouter | OpenAI-compatible | `openrouter.ai/api/v1/chat/completions` |
| Ollama | OpenAI-compatible | `localhost:11434/v1/chat/completions` |
| **Custom** | OpenAI-compatible | any URL you set |

Any provider with an OpenAI-compatible `/v1/chat/completions` endpoint works —
set Backend to `Custom (OpenAI-compatible)` and fill in Base URL + API Key.

### Production: NVS provisioning (no secrets in firmware)

```bash
scripts/provision.sh \
  --ssid "MyWiFi" --pass "secret" \
  --llm-backend 1 \
  --llm-key "sk-..." \
  --llm-url "https://your-provider.com/v1" \
  --llm-model "gpt-4o-mini"
```

## Serial CLI

ESPClaw provides local commands that are handled immediately without calling the LLM:

| Command | Description |
|---------|-------------|
| `/help` | Show available commands |
| `/tools` | List registered tools (18 tools) |
| `/heap` | Show free heap memory |
| `/gpio` | Show allowed GPIO pin range |
| `/reset` | Software reset |

Any other input is sent to the LLM agent for processing.

## Features

### Built-in Tools (18 total)

| Category | Tools |
|----------|-------|
| GPIO | `gpio_write`, `gpio_read`, `gpio_read_all` (3) |
| Memory | `memory_set`, `memory_get`, `memory_delete` (3) |
| Cron | `cron_schedule`, `cron_list`, `cron_cancel`, `cron_cancel_all` (4) |
| Time | `get_time`, `set_timezone` (2) |
| System | `get_diagnostics` (1) |

### Scheduled Tasks

ESPClaw supports natural language scheduling:

```
espclaw> remind me every 15 seconds to check
espclaw> remind me every day at 9am to stand up
espclaw> remind me in 30 minutes to check the oven
espclaw> cancel all tasks
```

**Features:**
- Periodic, daily, and one-time tasks
- Second-level precision (minimum 10s)
- Timezone support (UTC, Asia/Shanghai, etc.)
- NTP time sync
- NVS persistence (survives reboots)

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Channels (条件编译)                           │
│  Serial │ Telegram │ MQTT │ DingTalk │ Discord │ Slack │ ...     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │   Message Bus     │ FreeRTOS Queue
                    │  inbound/outbound │
                    └─────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │   Agent Loop      │ ReAct 循环
                    │  ┌─────────────┐  │
                    │  │  Session    │  │ 对话历史
                    │  │  Context    │  │ 系统提示词
                    │  └─────────────┘  │
                    └─────────┬─────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────▼───────┐    ┌────────▼────────┐    ┌──────▼──────┐
│    Provider   │    │  Tool Registry  │    │   Service   │
│  ┌─────────┐  │    │  ┌───────────┐  │    │ ┌─────────┐ │
│  │Anthropic│  │    │  │gpio_write │  │    │ │  cron   │ │
│  │ OpenAI  │  │    │  │gpio_read  │  │    │ │ratelimit│ │
│  │ Ollama  │  │    │  │memory_*   │  │    │ │bootguard│ │
│  └─────────┘  │    │  │cron_*     │  │    │ │   ota   │ │
└───────────────┘    │  │diagnostics│  │    │ └─────────┘ │
                     │  └───────────┘  │    └─────────────┘
                     └────────┬────────┘
                              │
                    ┌─────────▼─────────┐
                    │       HAL         │ 安全护栏
                    │  GPIO │ I2C │ PWM │
                    └───────────────────┘
```

### Module Matrix

> **状态图例:** ✅ 已验证 | 🔄 开发中 | 📋 计划中

| 分类 | 组件 | 文件 | 说明 |
|------|------|------|------|
| **Channels** | Serial | `channel_serial.c` | 串口控制台 ✅ 已验证 |
| | Telegram | `channel_telegram.c` | Bot API (long polling) 🔄 开发中 |
| | DingTalk | `channel_dingtalk.c` | 钉钉机器人 Webhook 🔄 开发中 |
| | Discord | `channel_discord.c` | Discord Webhook 🔄 开发中 |
| | Slack | `channel_slack.c` | Slack Incoming Webhook 🔄 开发中 |
| | WeCom | `channel_wecom.c` | 企业微信群机器人 🔄 开发中 |
| | Lark | `channel_lark.c` | 飞书机器人 (签名验证) 🔄 开发中 |
| | Pushplus | `channel_pushplus.c` | 推送加服务 🔄 开发中 |
| | Bark | `channel_bark.c` | iOS 推送通知 🔄 开发中 |
| | MQTT | `channel_mqtt.c` | IoT 标准协议 (双向) 🔄 开发中 |
| **Agent** | ReAct Loop | `agent/agent_loop.c` | 多轮工具调用循环 ✅ |
| | Session | `agent/session.c` | 对话历史管理 ✅ |
| | Context | `agent/context_builder.c` | 系统提示词组装 ✅ |
| | Persona | `agent/persona.c` | AI 人格切换 ✅ |
| **Provider** | Anthropic | `provider_anthropic.c` | Messages API ✅ |
| | OpenAI | `provider_openai.c` | Chat Completions API ✅ |
| **Tools** | GPIO | `tool_gpio.c` | 3个工具: write/read/read_all ✅ |
| | Memory | `tool_memory.c` | 3个工具: set/get/delete ✅ |
| | Cron | `tool_cron.c` | 4个工具: schedule/list/cancel/cancel_all ✅ |
| | Time | `tool_cron.c` | 2个工具: get_time/set_timezone ✅ |
| | System | `tool_system.c` | 1个工具: diagnostics ✅ |
| **Services** | Cron | `service/cron_service.c` | 定时任务调度器 ✅ |
| | Rate Limit | `util/ratelimit.c` | API 调用限制 ✅ |
| | WiFi | `manager/wifi_manager.c` | WiFi 连接管理 ✅ |
| | NVS | `manager/nvs_manager.c` | 键值存储 ✅ |
| **HAL** | GPIO | `hal/hal_gpio.c` | GPIO 安全护栏 ✅ |
| **Util** | JSON | `util/json_util.c` | 轻量 JSON 解析 (无 cJSON) ✅ |
| | HTTP | `util/http_client.c` | HTTPS 客户端封装 ✅ |

### Tools Detail (18 total)

| 工具名 | 参数 | 功能 |
|--------|------|------|
| `gpio_write` | `pin`, `state` | 设置 GPIO 高低电平 |
| `gpio_read` | `pin` | 读取 GPIO 状态 |
| `gpio_read_all` | - | 读取所有允许的 GPIO |
| `memory_set` | `key`, `value` | 持久化存储 (NVS) |
| `memory_get` | `key` | 读取存储的值 |
| `memory_delete` | `key` | 删除存储的键 |
| `cron_schedule` | `type`, `action`, `interval_seconds`/`hour`/`delay_seconds` | 创建定时任务 |
| `cron_list` | - | 列出所有任务 |
| `cron_cancel` | `id` | 取消指定任务 |
| `cron_cancel_all` | - | 取消所有任务 |
| `get_time` | - | 获取当前时间和 NTP 状态 |
| `set_timezone` | `timezone` | 设置时区 |
| `get_diagnostics` | - | 获取系统诊断信息 |
| `set_persona` | `persona` | 设置 AI 人格 (neutral/friendly/technical/witty) |
| `get_persona` | - | 获取当前 AI 人格设置 |
| `wifi_scan` | - | 扫描附近 WiFi 网络（会短暂断网）|
| `get_network_info` | - | 获取网络状态（IP/网关/DNS/MAC/信号）|

### File Structure

```
main/
├── main.c                 # 入口点，初始化各模块
├── CMakeLists.txt         # 构建配置
├── config.h               # 编译时常量
├── platform.h             # C3/S3/C5 条件编译宏
├── messages.h             # 消息队列类型定义
├── nvs_keys.h             # NVS 键名定义
│
├── agent/
│   ├── agent_loop.c       # ReAct 循环
│   ├── session.h/.c       # 对话历史
│   └── context_builder.h/.c
│   └── persona.h/.c       # AI 人格系统
│
├── channel/
│   ├── channel.h          # Channel vtable 接口
│   ├── channel_registry.c # 通道注册
│   ├── channel_serial.c   # 串口通道
│   ├── channel_telegram.c # Telegram Bot
│   ├── channel_dingtalk.c # 钉钉
│   ├── channel_discord.c  # Discord
│   ├── channel_slack.c    # Slack
│   ├── channel_wecom.c    # 企业微信
│   ├── channel_lark.c     # 飞书
│   ├── channel_pushplus.c # 推送加
│   ├── channel_bark.c     # iOS 推送
│   └── channel_mqtt.c     # MQTT IoT 控制
│
├── provider/
│   ├── provider.h         # Provider vtable
│   ├── provider_anthropic.c
│   └── provider_openai.c
│
├── tool/
│   ├── tool.h             # Tool 接口
│   ├── tool_registry.c    # 工具注册 + 分发
│   ├── builtin_tools.def  # X-macro 工具表
│   ├── tool_gpio.c        # GPIO 工具
│   ├── tool_memory.c      # 内存工具
│   ├── tool_cron.c        # 定时任务工具
│   └── tool_system.c      # 系统诊断
│   ├── tool_persona.c     # AI 人格切换
│   ├── tool_network.c     # 网络扫描
│
├── service/
│   ├── cron_service.h/.c  # 定时任务服务
│   └── (boot_guard.c)     # Step 9: 启动保护
│
├── manager/
│   ├── wifi_manager.h/.c  # WiFi 管理
│   └── nvs_manager.h/.c   # NVS 封装
│
├── hal/
│   └── hal_gpio.h/.c      # GPIO HAL + 安全护栏
│
└── util/
    ├── json_util.h/.c     # JSON 解析
    ├── http_client.h/.c   # HTTP 客户端
    └── ratelimit.h/.c     # 速率限制
```

## Development Plan

See [PLAN.md](PLAN.md) for the incremental step-by-step roadmap.
