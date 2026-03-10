# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![语言: C](https://img.shields.io/badge/语言-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![目标芯片](https://img.shields.io/badge/目标-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()
[![工具](https://img.shields.io/badge/工具-20个-orange.svg)]()
[![通道](https://img.shields.io/badge/通道-10个-purple.svg)]()
[![代码量](https://img.shields.io/badge/代码-8.5K行-lightgrey.svg)]()

[English](README.md)

基于 ESP-IDF 5.5 的纯 C AI 助手固件，在 $2 的 ESP32-C3/C5/S3 上运行 ReAct Agent，20 个工具 + 10 个通知通道，无需 PSRAM。

## 演示

<video src="https://github.com/tianrking/espclaw/raw/main/media/esp32c5.mp4" width="100%" controls></video>

### 持久记忆

ESPClaw 通过 NVS 存储在重启后仍能记住用户偏好：

<img src="media/memory.png" width="80%" alt="Memory demo - LLM 记住用户名">

### MQTT 通道

ESPClaw 支持双向 MQTT 通信，可对接 Home Assistant、Node-RED 等 IoT 平台：

<img src="media/espc5_mqtt.png" width="80%" alt="MQTT demo - ESP32-S3 与 MQTTX 交互">

## 支持的目标芯片

| 目标 | 核心数 | SRAM | PSRAM | Flash | 配置档 |
|------|--------|------|-------|-------|--------|
| ESP32-C3 | 1 | 400KB | 无 | 4MB | MINIMAL |
| ESP32-C5 | 1 | 400KB | 无 | 4MB | MINIMAL |
| ESP32-S3 | 2 | 512KB | 可选 | 4MB/16MB | MINIMAL/FULL |

> **注意：** 无 PSRAM 的 ESP32-S3 运行 MINIMAL 配置（与 C3/C5 相同）。带 8MB PSRAM 时解锁 FULL 功能（LittleFS、WebSocket、HTTP 代理）。

## 快速开始

```bash
# 设置目标芯片（首次或切换后）
idf.py set-target esp32s3   # 或 esp32c3 / esp32c5

# 配置
idf.py menuconfig
# -> ESPClaw Configuration -> LLM Settings（API Key、模型等）
# -> ESPClaw Configuration -> Channels -> Enable MQTT（可选）
# -> ESPClaw Configuration -> Channels -> Telegram（可选）

# 编译、烧录、监控
idf.py build flash monitor
```

> **重要：** 切换目标芯片后必须先删除 `sdkconfig`：
> ```bash
> rm -rf sdkconfig build && idf.py set-target esp32s3 && idf.py build
> ```

### ESP32-S3 配置

**无 PSRAM 的 ESP32-S3**（4MB Flash，普通开发板）：

```bash
# sdkconfig.defaults.esp32s3 已配置好 4MB Flash
idf.py set-target esp32s3
idf.py menuconfig  # 配置 WiFi + LLM API
idf.py build flash monitor
```

**带 PSRAM 的 ESP32-S3**（16MB Flash + 8MB PSRAM）：

```bash
idf.py menuconfig
# -> Serial Flasher Config -> Flash size -> 16MB
# -> Component Config -> ESP PSRAM -> Enable PSRAM
```

## LLM 配置

支持**任何 OpenAI 兼容的 API 端点**以及官方 Anthropic API。

### 支持的后端

| 后端 | 协议格式 | 默认端点 |
|------|----------|---------|
| Anthropic | Messages API | `api.anthropic.com/v1/messages` |
| OpenAI | Chat Completions | `api.openai.com/v1/chat/completions` |
| OpenRouter | OpenAI 兼容 | `openrouter.ai/api/v1/chat/completions` |
| Ollama | OpenAI 兼容 | `localhost:11434/v1/chat/completions` |
| **Custom** | OpenAI 兼容 | 任意 URL |

已测试：GPT-4o-mini、Claude 3 Haiku、GLM-4.5、DeepSeek、Qwen 等。

### 自定义 LLM 示例（GLM 智谱）

```bash
idf.py menuconfig
# -> ESPClaw Configuration -> LLM Settings
#    -> LLM Backend: Custom (OpenAI-compatible)
#    -> API Key: your_glm_api_key
#    -> Base URL: https://open.bigmodel.cn/api/paas/v4/chat/completions
#    -> Model name: glm-4-flash
```

## 功能特性

### 内置工具（20 个）

| 分类 | 工具 | 数量 |
|------|------|------|
| GPIO | `gpio_write`, `gpio_read`, `gpio_read_all`, `delay` | 4 |
| 内存 | `memory_set`, `memory_get`, `memory_delete`, `memory_list` | 4 |
| 定时 | `cron_schedule`, `cron_list`, `cron_cancel`, `cron_cancel_all` | 4 |
| 时间 | `get_time`, `set_timezone` | 2 |
| 系统 | `get_diagnostics`, `get_version` | 2 |
| 人格 | `set_persona`, `get_persona` | 2 |
| 网络 | `wifi_scan`, `get_network_info` | 2 |

### 通知通道（10 个）

> **状态说明：** Serial、Telegram 和 MQTT 已完整测试并稳定运行。其他通道代码结构完整，但尚未在真实服务上验证。

| 通道 | 类型 | 协议 | 状态 |
|------|------|------|------|
| Serial | 双向 | UART 串口控制台（默认启用）| **已验证** |
| Telegram | 双向 | Bot API 长轮询 | **已验证** |
| MQTT | 双向 | IoT 标准协议（Home Assistant / Node-RED）| **已验证** |
| 钉钉 | 单向发送 | Webhook + HMAC 签名 | 开发中 |
| Discord | 单向发送 | Webhook | 开发中 |
| Slack | 单向发送 | Incoming Webhook | 开发中 |
| 企业微信 | 单向发送 | 群机器人 Webhook | 开发中 |
| 飞书 | 单向发送 | 机器人 + 签名验证 | 开发中 |
| Pushplus | 单向发送 | 统一推送服务 | 开发中 |
| Bark | 单向发送 | iOS 推送通知 | 开发中 |

所有通道通过 `CONFIG_ESPCLAW_CHANNEL_xxx` 条件编译按需启用。

## MQTT 配置

### 启用 MQTT 通道

```bash
idf.py menuconfig
# -> ESPClaw Configuration -> Channels
#    -> [*] Enable MQTT Channel
#    -> MQTT Broker URL: mqtt://broker.emqx.io
#    -> MQTT Username:（匿名访问留空）
#    -> MQTT Password:（匿名访问留空）
```

### MQTT Topic 格式

ESPClaw 使用以下 topic 模式：

| 方向 | Topic 模式 |
|------|------------|
| 订阅（接收命令）| `espclaw/{client_id}/cmd` |
| 发布（发送响应）| `espclaw/{client_id}/response` |

`{client_id}` 从 MAC 地址自动生成，例如 `espclaw_feffe2`。

### 使用 MQTTX 测试

1. 安装 MQTTX: `brew install --cask mqttx`
2. 连接到 `mqtt://broker.emqx.io:1883`
3. 订阅: `espclaw/+/response`（或具体 client ID）
4. 发送到: `espclaw/{client_id}/cmd`，消息内容如 "你好"

### 使用命令行测试

```bash
# 安装 mosquitto 客户端
brew install mosquitto

# 订阅 ESP32 的响应
mosquitto_sub -h broker.emqx.io -t "espclaw/+/response" -v

# 发送命令到 ESP32（{client_id} 从串口日志中查看）
mosquitto_pub -h broker.emqx.io -t "espclaw/espclaw_feffe2/cmd" -m "你好"
```

### 公共 MQTT Broker

| Broker | URL | 说明 |
|--------|-----|------|
| EMQX | `mqtt://broker.emqx.io` | 推荐，无需认证 |
| Mosquitto | `mqtt://test.mosquitto.org` | 备选 |
| HiveMQ | `mqtt://broker.hivemq.com` | 备选 |

生产环境建议使用自建 MQTT Broker 或云服务（AWS IoT、Azure IoT 等），并启用 TLS（`mqtts://`）。

### 定时任务

支持自然语言创建定时任务，秒级精度：
```
espclaw> 每15秒提醒我检查
espclaw> 每天早上9点提醒我站起来
espclaw> 30分钟后提醒我查看烤箱
```

### 串口命令

| 命令 | 说明 |
|------|------|
| `/help` | 显示可用命令 |
| `/tools` | 列出已注册工具 |
| `/heap` | 显示剩余堆内存 |
| `/gpio` | 显示允许的 GPIO 引脚范围 |
| `/reset` | 软件重启 |

## 架构概述

```
+---------------------------------------------------------------------+
|                        Channels (条件编译)                             |
|  Serial | Telegram | MQTT | 钉钉 | Discord | Slack | ...           |
+-----------------------------+---------------------------------------+
                              |
                    +---------v---------+
                    |   Message Bus     | FreeRTOS Queue
                    |  inbound/outbound |
                    +---------+---------+
                              |
                    +---------v---------+
                    |   Agent Loop      | ReAct 循环
                    |  Session + Context|
                    +---------+---------+
                              |
        +---------------------+---------------------+
        |                     |                     |
+-------v-------+    +-------v--------+    +-------v------+
|    Provider   |    |  Tool Registry |    |   Service    |
|  Anthropic    |    |  20 个工具     |    |  cron        |
|  OpenAI       |    |  GPIO/Memory/  |    |  ratelimit   |
|  Ollama       |    |  Cron/Network  |    |  wifi_mgr    |
+---------------+    +-------+--------+    +--------------+
                             |
                    +--------v--------+
                    |      HAL        | 安全护栏
                    |  GPIO 白名单    |
                    +-----------------+
```

### 代码统计

- **8,478 行**纯 C 代码（59 个源文件）
- **零依赖**第三方 AI 库
- 固件大小：~920KB（4MB 分区剩余 78%）

### 文件结构

```
main/
├── main.c                    # 入口
├── config.h                  # 编译时常量
├── platform.h                # C3/C5/S3 条件编译
├── messages.h                # 消息队列类型
├── agent/
│   ├── agent_loop.c          # ReAct 循环 + 工具调度
│   ├── session.h/.c          # 对话历史
│   ├── context_builder.h/.c  # 系统提示词组装
│   └── persona.h/.c          # AI 人格系统
├── channel/
│   ├── channel.h             # Channel vtable 接口
│   ├── channel_serial.c      # 串口控制台
│   ├── channel_telegram.c    # Telegram Bot（长轮询）
│   ├── telegram_helpers.h/.c # Telegram 工具函数
│   ├── channel_mqtt.c        # MQTT IoT 通道
│   ├── channel_dingtalk.c    # 钉钉 Webhook
│   ├── channel_discord.c     # Discord Webhook
│   ├── channel_slack.c       # Slack Webhook
│   ├── channel_wecom.c       # 企业微信
│   ├── channel_lark.c        # 飞书
│   ├── channel_pushplus.c    # Pushplus
│   └── channel_bark.c        # iOS 推送 (Bark)
├── provider/
│   ├── provider.h            # Provider vtable
│   ├── provider_anthropic.c  # Anthropic Messages API
│   └── provider_openai.c     # OpenAI/OpenRouter/Ollama
├── tool/
│   ├── tool.h                # Tool 接口
│   ├── tool_registry.h/.c    # 注册 + 调度
│   ├── builtin_tools.def     # X-macro 工具表
│   ├── tool_gpio.c           # GPIO 工具 (4)
│   ├── tool_memory.c         # 内存工具 (4)
│   ├── tool_cron.c           # 定时 + 时间工具 (6)
│   ├── tool_system.c         # 系统工具 (2)
│   ├── tool_persona.c        # 人格工具 (2)
│   └── tool_network.c        # 网络工具 (2)
├── bus/
│   └── message_bus.h/.c      # 入站/出站队列
├── service/
│   ├── cron_service.h/.c     # 任务调度器
│   └── ratelimit.h/.c        # API 速率限制
├── mem/
│   └── nvs_manager.h/.c      # NVS 键值存储
└── hal/
    └── hal_gpio.h/.c         # GPIO HAL + 安全护栏
```

## WiFi 注意事项

**ESP32-C5：** 路由器需设置为 WPA2-PSK 或 WPA2/WPA3 混合模式。"仅 WPA"模式会导致认证失败。

## 开发计划

详见 [PLAN.md](PLAN.md)，包含完整的增量开发路线图。

## License

MIT
