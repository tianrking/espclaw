# ESPClaw 增量开发计划

## 项目定位

ESP-IDF 5.5 纯 C AI 助手固件，主攻 ESP32-C3，兼容 ESP32-S3/C5。
参考项目：zclaw (888KB极限), mimiclaw (S3全功能), zeroclaw (Rust服务器版)

## 核心规则

- **每一步都必须能编译 + 烧录 + 验证**
- 新模块逐个加入 CMakeLists.txt，不提前引用不存在的文件
- C3 优先，S3 特性用 `#if ESPCLAW_HAS_xxx` 条件编译
- Channel 按需启用，节省 Flash/RAM

---

## 当前进度

- [x] Step 1 — 最小可编译固件 (已验证烧录)
- [x] Step 2 — WiFi 连接 (已验证，注意 WPA 版本兼容性)
- [x] Step 3 — 串口交互 (已验证，C5 上运行正常)
- [x] Step 4 — LLM 对话 (已验证，支持 Anthropic / OpenAI 兼容接口)
- [x] Step 5 — ReAct Agent (已验证，多轮对话 + 历史记录)
- [x] Step 6 — 工具系统 (已验证，GPIO + Memory + 7个工具)
- [x] Step 7 — 多通道系统 (9个通知通道已完成)
- [x] Step 8 — 定时任务与速率限制 (已完成)
- [ ] Step 9 — 生产就绪 ← 当前
- [ ] Step 10 — S3 全功能

---

## Step 1-6: 基础功能 ✅ (已完成)

详见历史记录。核心模块：
- WiFi Manager / NVS Manager
- Message Bus / Channel 系统
- LLM Provider (Anthropic/OpenAI)
- ReAct Agent + Session
- Tool Registry + 7个内置工具
- GPIO HAL + 安全护栏

---

## Step 7: 多通道系统 ✅ (已完成)

### 已实现的通道

| 通道 | 类型 | 说明 |
|------|------|------|
| Serial | 双向 | 串口控制台 (默认启用) |
| Telegram | 双向 | Bot API (long polling) |
| DingTalk | 单向 | 钉钉机器人 Webhook (HMAC签名) |
| Discord | 单向 | Discord Webhook |
| Slack | 单向 | Slack Incoming Webhook |
| WeCom | 单向 | 企业微信群机器人 (Markdown) |
| Lark | 单向 | 飞书机器人 (签名验证) |
| Pushplus | 单向 | 推送加 (统一推送服务) |
| Bark | 单向 | iOS 推送通知 |
| MQTT | 双向 | IoT 标准协议 (Home Assistant/Node-RED) |

### 架构特点
- 条件编译：`CONFIG_ESPCLAW_CHANNEL_xxx` 控制是否编译
- 统一接口：`channel_ops_t` vtable
- CMakeLists 按需添加源文件

---

## Step 8: 定时任务与速率限制 ✅ (已完成)

### 8.1 速率限制
**文件:** `util/ratelimit.h` + `.c`

**功能:**
- LLM API 调用限制：100次/小时，1000次/天
- NVS 持久化（重启后恢复日计数）
- 超限返回友好提示 `[rate limited]`

### 8.2 定时任务
**文件:** `service/cron_service.h` + `.c`, `tool/tool_cron.c`

**功能:**
- 三种调度类型：periodic (周期), daily (每日), once (一次性)
- 秒级精度 (最小 10 秒)
- NTP 时间同步 + 时区支持
- NVS 持久化（重启后恢复任务）

**工具:**
| 工具 | 功能 |
|------|------|
| `cron_schedule` | 创建定时任务 (支持秒/分钟) |
| `cron_list` | 列出所有任务 |
| `cron_cancel` | 取消单个任务 |
| `cron_cancel_all` | 取消所有任务 |
| `get_time` | 获取当前时间 + NTP 状态 |
| `set_timezone` | 设置时区 |

**验证:**
```
espclaw> remind me every 15 seconds to check
espclaw> list all tasks
espclaw> cancel all tasks
```

---

## Step 9: 生产就绪

### 9.1 启动保护 (参考 zclaw)
**新增文件:**
- `service/boot_guard.h` + `.c`

**功能:**
- 记录启动次数到 NVS
- 连续崩溃 >4 次进入安全模式
- 安全模式：只启动串口通道，不启动网络

### 9.2 OTA 升级
**新增文件:**
- `service/ota.h` + `.c`

**功能:**
- 双分区 A/B 切换
- HTTPS 固件下载
- 版本验证
- 回滚保护

### 9.3 安全加固
**新增文件:**
- `security/security.h` + `.c`

**功能:**
- NVS 加密存储敏感数据
- API key 运行时注入（不硬编码）
- Flash 加密支持

### 9.4 增强诊断
**新增工具:**
- `get_diagnostics` 增强：增加 rate limit 状态、cron 任务统计、uptime

**验证:**
```bash
idf.py size
# 确认固件 < 888KB (zclaw 兼容目标)
```

---

## Step 10: S3 全功能

### 10.1 文件系统 (S3 only)
**新增文件:**
- `mem/storage.h` + `.c` — LittleFS 管理

**功能:**
- SOUL.md — AI 人格定义
- USER.md — 用户偏好
- MEMORY.md — 长期记忆
- session/ — 会话持久化

### 10.2 WebSocket Gateway (S3 only)
**新增文件:**
- `channel/channel_ws.h` + `.c`

**功能:**
- 本地 WebSocket 服务 (port 18789)
- 配合 Web UI 使用
- 双向通信

### 10.3 心跳服务 (S3 only)
**新增文件:**
- `service/heartbeat.h` + `.c`

**功能:**
- 定期检查 HEARTBEAT.md
- 自主执行预设任务
- 30 分钟间隔

### 10.4 高级 HAL
**新增文件:**
- `hal/hal_spi.h` + `.c`
- `tool/tool_files.c`

---

## Step 11: 高级功能 (可选)

### 11.1 I2C 工具
**新增文件:**
- `hal/hal_i2c.h` + `.c`
- `tool/tool_i2c.c`

**工具:**
- `i2c_scan` — 扫描 I2C 总线设备
- `i2c_read` — 读取寄存器
- `i2c_write` — 写入寄存器

### 11.2 PWM 工具 (P0 - 智能家居核心)
**新增文件:**
- `hal/hal_pwm.h` + `.c`
- `tool/tool_pwm.c`

**工具:**
- `pwm_set` — 设置 PWM 通道 (频率 + 占空比)
- `pwm_set_freq` — 设置频率
- `pwm_set_duty` — 设置占空比 (0-100%)
- `pwm_stop` — 停止 PWM 输出

**应用场景:** LED 调光、电机调速、蜂鸣器

### 11.3 ADC 工具 (P0 - 智能家居核心)
**新增文件:**
- `hal/hal_adc.h` + `.c`
- `tool/tool_adc.c`

**工具:**
- `adc_read` — 读取 ADC 原始值
- `adc_read_voltage` — 读取电压值 (mV)
- `adc_read_all` — 读取所有 ADC 通道

**应用场景:** 光敏传感器、温度传感器、电池电量、电位器

### 11.4 人格系统 (参考 zclaw)
**新增文件:**
- `tool/tool_persona.c`

**功能:**
- 四种人格：neutral, friendly, technical, witty
- 影响系统提示词风格
- 运行时切换

### 11.5 用户自定义工具 (参考 zclaw)
**新增文件:**
- `tool/tool_user.h` + `.c`

**功能:**
- LLM 通过自然语言创建新工具
- 工具定义存储在 NVS
- 动态注册到 tool registry

### 11.6 网络搜索 (可选方案)
**方案 A: 服务端搜索 (推荐)**
- 使用 Perplexity API (sonar-online 模型)
- 零代码，LLM 自带联网能力
- 更换 LLM Backend 为 Perplexity 即可

**方案 B: 本地搜索 (参考 mimiclaw)**
**新增文件:**
- `tool/tool_web_search.c`

**功能:**
- Brave Search API 集成
- PSRAM 缓冲响应
- 代理支持 (复杂网络环境)

**工具:**
- `web_search` — 搜索互联网获取实时信息

---

## 硬件工具路线图

### 设计理念
ESPClaw 的核心差异化优势是 **硬件交互能力**。通过自然语言控制真实硬件，
实现真正的"语音控制智能家居"。

### 优先级排序

| 优先级 | 工具 | 复杂度 | 覆盖场景 | 状态 |
|--------|------|--------|----------|------|
| P0 | GPIO | ★☆☆ | 开关控制 (继电器/LED) | ✅ 已完成 |
| P0 | PWM | ★★☆ | 调光/调速 (LED/电机) | 📋 计划中 |
| P0 | ADC | ★★☆ | 模拟传感器 (光/温度/电量) | 📋 计划中 |
| P1 | I2C | ★★★ | 数字传感器 (SHT30/BME280) | 📋 计划中 |
| P2 | SPI | ★★★★ | 高速外设 (屏幕/Flash) | 📋 计划中 |

### 智能家居用例

| 场景 | 所需工具 | 示例命令 |
|------|----------|----------|
| 灯光开关 | GPIO + 继电器 | "打开客厅灯" |
| 灯光调光 | PWM | "把灯调到 50%" |
| 风扇调速 | PWM | "风扇开中档" |
| 环境监测 | ADC/I2C + 传感器 | "现在的温度湿度是多少" |
| 光照检测 | ADC + 光敏电阻 | "现在外面亮吗" |
| 浇花系统 | GPIO + 电磁阀 | "每天早上 7 点浇花" |
| 安防报警 | GPIO + ADC | "门窗被打开时通知我" |

### 架构特点
- **安全护栏:** 所有硬件操作通过 HAL 层，限制可用引脚
- **动态配置:** 频率/引脚通过工具参数传入，无需重新编译
- **按需启用:** 通过 Kconfig 选择性编译硬件工具，节省 Flash

---

## 功能对比矩阵

---

## 深度架构分析：为什么 ESPClaw 更优

### 竞品问题与 ESPClaw 解决方案

#### zclaw (888KB 极限固件)

**优点:**
- 极致体积控制，888KB 封顶
- GPIO 安全护栏 (白名单机制)
- 人格系统 (4种风格)
- 启动保护 (崩溃恢复)

**问题:**
- ❌ 只支持 ESP32 原版，不支持 C3/C5/S3
- ❌ 通道只有 2 个 (Serial + Telegram)
- ❌ 无 MQTT / 钉钉 / 飞书等企业通道
- ❌ 无硬件扩展路线 (PWM/ADC/I2C)

**ESPClaw 解决:**
- ✅ 支持 C3/C5/S3 三种芯片
- ✅ 10 个通道，覆盖企业场景
- ✅ 硬件工具路线图 (PWM/ADC/I2C/SPI)
- ✅ 保留 GPIO 安全护栏

---

#### mimiclaw (S3 全功能)

**优点:**
- 文件系统 (SOUL.md / MEMORY.md)
- 心跳服务 (自主执行任务)
- Web 搜索 (Brave Search API)
- WebSocket Gateway
- OTA 升级
- 双核调度

**问题:**
- ❌ **强制 PSRAM** — 必须用 $5 的 S3，不能用 $2 的 C3
- ❌ 通道只有 2 个 (Serial + Telegram)
- ❌ 无 GPIO 安全护栏 (LLM 可能误操作)
- ❌ 无企业通知通道

**ESPClaw 解决:**
- ✅ **无需 PSRAM** — C3/C5 400KB SRAM 即可运行
- ✅ 10 个通道，包含中国企业专属通道
- ✅ GPIO 安全护栏，防止变砖
- ✅ 计划支持文件系统 (S3 配置)

---

#### zeroclaw (Rust 服务器版)

**优点:**
- Rust 安全 + 高性能
- Trait 架构，一切可替换
- 25+ 通道，75+ 工具
- 5MB RAM 极低内存
- 跨平台 (ARM/x86/RISC-V)

**问题:**
- ❌ **需要服务器** — 无法运行在 MCU
- ❌ 无真实硬件控制 (GPIO/PWM/ADC)
- ❌ 需要持续供电 + 网络
- ❌ 部署复杂度高

**ESPClaw 解决:**
- ✅ **MCU 原生运行** — 无需服务器
- ✅ 真实硬件控制 (继电器/传感器)
- ✅ USB 供电，0.5W 功耗
- ✅ 一键烧录，即插即用

---

### 架构对比总结

| 维度 | zclaw | mimiclaw | zeroclaw | **ESPClaw** |
|------|-------|----------|----------|-------------|
| **定位** | 极限体积 | S3 全功能 | 服务器框架 | **平衡最优** |
| **芯片** | ESP32 | S3 only | x86/ARM | **C3/C5/S3** |
| **成本** | $2 | $5 | $50+ | **$2** |
| **通道** | 2 | 2 | 25+ | **10** |
| **GPIO 安全** | ✅ | ✅ | ❌ | **✅** |
| **企业通道** | ❌ | ❌ | ✅ | **✅** |
| **硬件扩展** | ❌ | ❌ | ❌ | **✅ (PWM/ADC/I2C)** |
| **无需 PSRAM** | ✅ | ❌ | ❌ | **✅** |
| **OTA** | ❌ | ✅ | ✅ | **📋** |
| **文件系统** | ❌ | ✅ | ✅ | **📋 (S3)** |

---

### ESPClaw 核心优势

```
┌─────────────────────────────────────────────────────────────────────┐
│                    ESPClaw = 最佳平衡点                              │
├─────────────────────────────────────────────────────────────────────┤
│  成本: $2 (C3) ────────────────────────────────────────────── ✅    │
│  通道: 10 个 ───────────────────────────────────────────────── ✅    │
│  芯片: C3 / C5 / S3 ───────────────────────────────────────── ✅    │
│  安全: GPIO 白名单 ────────────────────────────────────────── ✅    │
│  扩展: PWM / ADC / I2C / SPI ──────────────────────────────── 📋    │
│  企业: 钉钉 / 飞书 / 企业微信 ─────────────────────────────── ✅    │
│  IoT:  MQTT (Home Assistant) ──────────────────────────────── ✅    │
└─────────────────────────────────────────────────────────────────────┘
```

**一句话定位:**
> ESPClaw 在 $2 的 ESP32-C3 上，实现了 mimiclaw 需要 $5 S3 的功能，
> 同时拥有 zeroclaw 级别的通道丰富度，和 zclaw 级别的 GPIO 安全护栏。

---

### 未来潜力

| 方向 | 状态 | 说明 |
|------|------|------|
| 本地 AI (唤醒词) | 📋 | ESP-SR 集成，S3 可跑 WakeNet |
| 视觉 AI (摄像头) | 📋 | S3 + OV2640，MobileNet 推理 |
| 文件系统 | 📋 | S3 LittleFS，SOUL.md / MEMORY.md |
| OTA 升级 | 📋 | 双分区 A/B，HTTPS 下载 |
| 心跳服务 | 📋 | 自主执行 HEARTBEAT.md 任务 |

---

### 硬件要求对比 (核心优势)

| 指标 | ESPClaw | zclaw | mimiclaw | zeroclaw |
|------|---------|-------|----------|----------|
| **最低硬件成本** | **~$2** (ESP32-C3) | ~$2 (ESP32) | ~$5 (ESP32-S3) | 服务器 (云/VPS) |
| **最低 RAM** | **400KB** (无PSRAM) | 400KB | 8MB PSRAM | 512MB+ |
| **最低 Flash** | **4MB** | 4MB | 16MB | 磁盘 |
| **芯片选择** | **C3 / C5 / S3** | ESP32 | S3 only | x86/ARM |

> 💡 **ESPClaw 核心价值**: 在 **$2 级别** 的 ESP32-C3 上实现完整 AI Agent + 10个通知通道 + 硬件控制，
> 同类项目需要 ESP32-S3 ($5) 或云服务器才能实现同等功能。

### 硬件控制对比 (差异化核心)

| 功能 | ESPClaw | zclaw | mimiclaw | zeroclaw |
|------|---------|-------|----------|----------|
| **GPIO 开关控制** | ✅ 安全护栏 | ✅ 安全护栏 | ⚠️ 无保护 | ❌ 无硬件 |
| **GPIO 安全引脚限制** | ✅ 白名单机制 | ✅ | ❌ | ❌ |
| **GPIO 批量读取** | ✅ `gpio_read_all` | ✅ | ❌ | ❌ |
| PWM 调光/调速 | 📋 | ❌ | ❌ | ❌ |
| ADC 模拟传感器 | 📋 | ❌ | ❌ | ❌ |
| I2C 数字传感器 | 📋 | ✅ | ❌ | ❌ |

> 🔒 **GPIO 安全护栏**: ESPClaw 与 zclaw 共有！限制可操作引脚范围，防止 LLM 误操作 Flash/PSRAM 引脚导致设备变砖。
> mimiclaw 无此保护，存在变砖风险。支持场景：继电器控制灯光/电器、电磁阀浇花、蜂鸣器报警等。

### 软件功能对比

| 功能 | ESPClaw | zclaw | mimiclaw | zeroclaw |
|------|---------|-------|----------|----------|
| 基础 LLM 对话 | ✅ | ✅ | ✅ | ✅ |
| ReAct Agent | ✅ | ✅ | ✅ | ✅ |
| GPIO 控制 | ✅ | ✅ | ✅ | ✅ |
| 持久记忆 | ✅ | ✅ | ✅ | ✅ |
| Serial 通道 | ✅ | ✅ | ✅ | - |
| Telegram | ✅ | ✅ | ✅ | ✅ |
| 钉钉 | ✅ | - | - | ✅ |
| Discord | ✅ | - | - | ✅ |
| Slack | ✅ | - | - | ✅ |
| 企业微信 | ✅ | - | - | - |
| 飞书 | ✅ | - | - | ✅ |
| 推送加 | ✅ | - | - | - |
| iOS推送 | ✅ | - | - | - |
| MQTT | ✅ | - | - | ✅ |
| 速率限制 | ✅ | ✅ | - | ✅ |
| 定时任务 | ✅ | ✅ | ✅ | ✅ |
| 启动保护 | 📋 | ✅ | - | - |
| OTA 升级 | 📋 | ✅ | ✅ | - |
| 人格切换 | 📋 | ✅ | - | - |
| I2C 工具 | 📋 | ✅ | - | ✅ |
| PWM 工具 | 📋 | - | - | ✅ |
| ADC 工具 | 📋 | - | - | ✅ |
| WebSocket | 📋 | ✅ | - | ✅ |
| 文件系统 | 📋 | - | ✅ | ✅ |
| 自定义工具 | 📋 | ✅ | - | - |
| 网络搜索 | 📋 | - | ✅ | ✅ |

图例：✅ 已实现 | 🔄 进行中 | 📋 计划中 | - 无

### 通道数量对比

| 项目 | 双向通道 | 单向通知 | 总计 |
|------|----------|----------|------|
| **ESPClaw** | 3 (Serial, Telegram, MQTT) | 7 (DingTalk, Discord, Slack, WeCom, Lark, Pushplus, Bark) | **10** |
| zclaw | 2 (Serial, Telegram) | 0 | 2 |
| mimiclaw | 2 (Serial, Telegram) | 0 | 2 |
| zeroclaw | 3+ | 5+ | 8+ |

---

## 架构总览

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
                    │  GPIO │ PWM │ ADC │ I2C │ SPI │
                    └───────────────────┘
```

---

## 参考源码

- **zclaw** (888KB 极限 C 固件):
  - `main/agent.c` — Agent 循环
  - `main/cron.c` — 定时任务
  - `main/ratelimit.c` — 速率限制
  - `main/boot_guard.c` — 启动保护
  - `main/tools_cron.c` — Cron 工具

- **mimiclaw** (S3 全功能):
  - `main/mimi.c` — 双核调度
  - `main/cron/` — 定时任务
  - `main/ota/` — OTA 升级
  - `main/skills/` — 技能系统

