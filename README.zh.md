# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![语言: C](https://img.shields.io/badge/语言-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![目标芯片](https://img.shields.io/badge/目标-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()
[![工具](https://img.shields.io/badge/工具-20个-orange.svg)]()
[![通道](https://img.shields.io/badge/通道-10个-purple.svg)]()
[![代码量](https://img.shields.io/badge/代码-8.5K行-lightgrey.svg)]()

[English](README.md)

基于 ESP-IDF 5.5 的纯 C AI 助手固件，在 $2 的 ESP32-C3/C5 上运行 ReAct Agent，20 个工具 + 10 个通知通道，无需 PSRAM。

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

## 快速开始

```bash
# 设置目标芯片（首次或切换后）
idf.py set-target esp32c5   # 或 esp32c3 / esp32s3

# 配置
idf.py menuconfig
# -> ESPClaw Configuration -> LLM Settings（API Key、模型等）
# -> ESPClaw Configuration -> Telegram（可选）

# 编译、烧录、监控
idf.py build flash monitor
```

> **重要：** 切换目标芯片后必须先删除 `sdkconfig`：
> ```bash
> rm sdkconfig && idf.py set-target esp32c5 && idf.py build
> ```

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

### 生产环境：NVS 烧录

```bash
scripts/provision.sh \
  --ssid "MyWiFi" --pass "secret" \
  --llm-backend 1 \
  --llm-key "sk-..." \
  --llm-url "https://your-provider.com/v1" \
  --llm-model "gpt-4o-mini"
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

| 通道 | 类型 | 协议 |
|------|------|------|
| Serial | 双向 | UART 串口控制台（默认启用）|
| Telegram | 双向 | Bot API 长轮询 |
| MQTT | 双向 | IoT 标准协议（Home Assistant / Node-RED）|
| 钉钉 | 单向发送 | Webhook + HMAC 签名 |
| Discord | 单向发送 | Webhook |
| Slack | 单向发送 | Incoming Webhook |
| 企业微信 | 单向发送 | 群机器人 Webhook |
| 飞书 | 单向发送 | 机器人 + 签名验证 |
| Pushplus | 单向发送 | 统一推送服务 |
| Bark | 单向发送 | iOS 推送通知 |

所有通道通过 `CONFIG_ESPCLAW_CHANNEL_xxx` 条件编译按需启用。

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

## WiFi 注意事项

**ESP32-C5：** 路由器需设置为 WPA2-PSK 或 WPA2/WPA3 混合模式。"仅 WPA"模式会导致认证失败。

## 开发计划

详见 [PLAN.md](PLAN.md)，包含完整的增量开发路线图。

## License

MIT
