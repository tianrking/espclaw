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
| `/tools` | List registered tools (14 tools) |
| `/heap` | Show free heap memory |
| `/gpio` | Show allowed GPIO pin range |
| `/reset` | Software reset |

Any other input is sent to the LLM agent for processing.

## Features

### Built-in Tools (14 total)

| Category | Tools |
|----------|-------|
| GPIO | `gpio_write`, `gpio_read`, `gpio_read_all` |
| Memory | `memory_set`, `memory_get`, `memory_delete` |
| Cron | `cron_schedule`, `cron_list`, `cron_cancel`, `cron_cancel_all` |
| Time | `get_time`, `set_timezone` |
| System | `get_diagnostics` |

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

## Development Plan

See [PLAN.md](PLAN.md) for the incremental step-by-step roadmap.
