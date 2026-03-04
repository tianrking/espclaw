# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![语言: C](https://img.shields.io/badge/语言-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![目标芯片](https://img.shields.io/badge/目标-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()

[🇺🇸 English](README.md)

基于 ESP-IDF 5.5 的纯 C AI 助手固件，支持 ESP32-C3 / C5 / S3。

## 演示

<video src="https://github.com/tianrking/espclaw/raw/main/media/esp32c5.mp4" width="100%" controls></video>

### 持久记忆

ESPClaw 通过 NVS 存储在重启后仍能记住用户偏好：

<img src="media/memory.png" width="80%" alt="Memory demo - LLM 记住用户名">

## 支持的目标芯片

| 目标 | 核心数 | PSRAM | Flash | 配置档 |
|------|--------|-------|-------|--------|
| ESP32-C3 | 1 | 无 | 4MB | MINIMAL |
| ESP32-C5 | 1 | 无 | 4MB | MINIMAL |
| ESP32-S3 | 2 | 8MB | 16MB | FULL |

## 编译与烧录

```bash
# 首次使用或切换目标芯片时，选择其中一条：
idf.py set-target esp32c5
idf.py set-target esp32c3
idf.py set-target esp32s3

idf.py build flash monitor
```

## 切换目标芯片

> **重要：** 切换目标芯片后必须先删除 `sdkconfig`，否则旧芯片缓存的 flash 大小和分区设置会导致编译失败。

```bash
# 示例：从 C3 切换到 C5
idf.py set-target esp32c5
rm sdkconfig          # ← 每次切换目标后必须执行
idf.py build
```

原因：`idf.py set-target` 只更新 `sdkconfig.defaults.*` 的选择，但本地 `sdkconfig` 文件优先级更高，会保留旧芯片的默认值（例如 C5 的 2MB flash 而非所需的 4MB）。删除后会强制从 `sdkconfig.defaults` + `sdkconfig.defaults.<target>` 重新生成。

## WiFi 注意事项

**WPA 版本兼容性（ESP32-C5）：** 若设备连接失败，请检查路由器的安全模式。ESP32-C5 的 WiFi 驱动在提供密码时默认要求 WPA2 或更高版本。设置为"仅 WPA"（非 WPA2）的路由器会导致认证失败，请将路由器安全模式切换为 **WPA2-PSK** 或 **WPA2/WPA3 混合模式**。

连接成功后驱动会打印安全类型：
```
wifi: security: WPA2-PSK, phy:bgn, rssi:-41
```

## LLM 配置

ESPClaw 除官方 Anthropic 和 OpenAI API 外，还支持**任何 OpenAI 兼容的 API 端点**（第三方服务商、自托管模型等）。

### 开发阶段：通过 menuconfig 快速配置

```bash
idf.py menuconfig
# → ESPClaw Configuration → LLM Settings
#   Backend:  0=Anthropic  1=OpenAI  2=OpenRouter  3=Ollama  4=Custom
#   API Key:  你的密钥
#   Base URL: https://your-provider.com/v1  （官方端点留空）
#   Model:    模型名称（如 gpt-4o-mini、claude-3-haiku、gemini-pro）
```

### 支持的后端

| 后端 | 协议格式 | 默认端点 |
|------|----------|---------|
| Anthropic | Messages API | `api.anthropic.com/v1/messages` |
| OpenAI | Chat Completions | `api.openai.com/v1/chat/completions` |
| OpenRouter | OpenAI 兼容 | `openrouter.ai/api/v1/chat/completions` |
| Ollama | OpenAI 兼容 | `localhost:11434/v1/chat/completions` |
| **Custom** | OpenAI 兼容 | 任意你填写的 URL |

任何提供 OpenAI 兼容 `/v1/chat/completions` 端点的服务商均可使用 —— 将 Backend 设为 `Custom (OpenAI-compatible)` 并填入 Base URL 和 API Key 即可。

### 生产环境：NVS 烧录（密钥不进固件）

```bash
scripts/provision.sh \
  --ssid "MyWiFi" --pass "secret" \
  --llm-backend 1 \
  --llm-key "sk-..." \
  --llm-url "https://your-provider.com/v1" \
  --llm-model "gpt-4o-mini"
```

## 串口命令

ESPClaw 提供本地命令，直接执行无需调用 LLM：

| 命令 | 说明 |
|------|------|
| `/help` | 显示可用命令 |
| `/tools` | 列出已注册工具（14个） |
| `/heap` | 显示剩余堆内存 |
| `/gpio` | 显示允许的 GPIO 引脚范围 |
| `/reset` | 软件重启 |

其他输入会发送给 LLM 进行处理。

## 功能特性

### 内置工具（14个）

| 分类 | 工具 |
|------|------|
| GPIO | `gpio_write`, `gpio_read`, `gpio_read_all` (3个) |
| 内存 | `memory_set`, `memory_get`, `memory_delete` (3个) |
| 定时 | `cron_schedule`, `cron_list`, `cron_cancel`, `cron_cancel_all` (4个) |
| 时间 | `get_time`, `set_timezone` (2个) |
| 系统 | `get_diagnostics` (1个) |

### 定时任务

ESPClaw 支持自然语言创建定时任务：

```
espclaw> 每15秒提醒我检查
espclaw> 每天9点提醒我站起来
espclaw> 30分钟后提醒我查看烤箱
espclaw> 删除所有任务
```

**功能特点：**
- 三种任务类型：周期、每日、一次性
- 秒级精度（最小10秒）
- 时区支持（UTC、Asia/Shanghai 等）
- NTP 时间同步
- NVS 持久化（重启后恢复）

## 架构概述

### 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Channels (条件编译)                           │
│  Serial │ Telegram │ DingTalk │ Discord │ Slack │ WeCom │ ...     │
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

### 模块矩阵

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
| **Agent** | ReAct Loop | `agent/agent_loop.c` | 多轮工具调用循环 ✅ |
| | Session | `agent/session.c` | 对话历史管理 ✅ |
| | Context | `agent/context_builder.c` | 系统提示词组装 ✅ |
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

### 工具详情 (共14个)

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

### 文件结构

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
│   └── channel_bark.c     # iOS 推送
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

## 开发计划

详见 [PLAN.md](PLAN.md)，包含完整的增量开发步骤路线图。
