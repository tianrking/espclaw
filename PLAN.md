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

### 11.2 PWM 工具
**新增文件:**
- `hal/hal_pwm.h` + `.c`
- `tool/tool_pwm.c`

**工具:**
- `pwm_set` — 设置 PWM 通道
- `pwm_set_freq` — 设置频率
- `pwm_set_duty` — 设置占空比

### 11.3 人格系统 (参考 zclaw)
**新增文件:**
- `tool/tool_persona.c`

**功能:**
- 四种人格：neutral, friendly, technical, witty
- 影响系统提示词风格
- 运行时切换

### 11.4 用户自定义工具 (参考 zclaw)
**新增文件:**
- `tool/tool_user.h` + `.c`

**功能:**
- LLM 通过自然语言创建新工具
- 工具定义存储在 NVS
- 动态注册到 tool registry

---

## 功能对比矩阵

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
| 速率限制 | ✅ | ✅ | - | ✅ |
| 定时任务 | ✅ | ✅ | ✅ | ✅ |
| 启动保护 | 📋 | ✅ | - | - |
| OTA 升级 | 📋 | ✅ | ✅ | - |
| 人格切换 | 📋 | ✅ | - | - |
| I2C 工具 | 📋 | ✅ | - | ✅ |
| PWM 工具 | 📋 | - | - | ✅ |
| WebSocket | 📋 | ✅ | - | ✅ |
| 文件系统 | 📋 | - | ✅ | ✅ |
| 自定义工具 | 📋 | ✅ | - | - |

图例：✅ 已实现 | 🔄 进行中 | 📋 计划中 | - 无

---

## 架构总览

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

---

## 关键文件索引

| 文件 | 作用 | 引入步骤 |
|------|------|---------|
| platform.h | C3/S3/C5 条件编译宏 | Step 1 |
| config.h | 所有编译时常量 | Step 1 |
| messages.h | 队列消息类型 | Step 3 |
| nvs_keys.h | NVS 键名 | Step 2 |
| provider.h | LLM Provider vtable | Step 4 |
| channel.h | Channel vtable | Step 3 |
| tool.h | Tool 接口 + 能力掩码 | Step 6 |
| hal_gpio.h | GPIO HAL vtable | Step 6 |
| Kconfig.projbuild | 配置菜单 | Step 7 |

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

- **zeroclaw** (Rust 服务器版):
  - `src/channels/` — 25+ 通道实现
  - `src/tools/` — 75+ 工具
  - `src/agent/` — Agent 系统
  - `src/cron/` — 定时任务
