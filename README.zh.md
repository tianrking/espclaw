# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![语言: C](https://img.shields.io/badge/语言-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![目标芯片](https://img.shields.io/badge/目标-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()

[🇺🇸 English](README.md)

基于 ESP-IDF 5.5 的纯 C AI 助手固件，支持 ESP32-C3 / C5 / S3。

## 演示

<video src="https://github.com/tianrking/espclaw/raw/main/media/esp32c5.mp4" width="100%" controls></video>

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

## 开发计划

详见 [PLAN.md](PLAN.md)，包含完整的增量开发步骤路线图。
