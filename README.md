# ESPClaw

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](main/)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.5-red.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![Target](https://img.shields.io/badge/Target-ESP32--C3%20%7C%20C5%20%7C%20S3-green.svg)]()

[🇨🇳 中文文档](README.zh.md)

ESP-IDF 5.5 AI assistant firmware for ESP32-C3 / C5 / S3.

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

## Development Plan

See [PLAN.md](PLAN.md) for the incremental step-by-step roadmap.
