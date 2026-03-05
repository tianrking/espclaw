# ESPClaw 增量开发计划

## 项目定位

ESP-IDF 5.5 纯 C AI 助手固件，支持 ESP32-C3 / C5 / S3。
在 $2 级别的 MCU 上实现完整 AI Agent + 20 个工具 + 10 个通知通道 + 硬件控制。

## 核心规则

- **每一步都必须能编译 + 烧录 + 验证**
- 新模块逐个加入 CMakeLists.txt，不提前引用不存在的文件
- C3/C5 优先，S3 特性用 `#if ESPCLAW_HAS_xxx` 条件编译
- Channel 按需启用，节省 Flash/RAM

---

## 当前进度

- [x] Step 1 — 最小可编译固件
- [x] Step 2 — WiFi 连接
- [x] Step 3 — 串口交互
- [x] Step 4 — LLM 对话（Anthropic / OpenAI 兼容接口）
- [x] Step 5 — ReAct Agent（多轮对话 + 历史记录）
- [x] Step 6 — 工具系统（20 个工具）
- [x] Step 7 — 多通道系统（10 个通道：Serial + Telegram 双向 + 8 个单向通知）
- [x] Step 8 — 定时任务 + 速率限制 + Telegram 稳定性优化
- [ ] Step 9 — 生产就绪（OTA + 启动保护）← 下一步
- [ ] Step 10 — S3 全功能（文件系统 + WebSocket）

**代码量：8,478 行 | 59 个源文件 | 固件 ~920KB**

---

## Step 1-4: 基础设施 (已完成)

- WiFi Manager / NVS Manager
- Message Bus + Serial Channel
- LLM Provider (Anthropic Messages API / OpenAI Chat Completions)
- 串口交互 + 本地命令 (`/help`, `/tools`, `/heap`, `/gpio`, `/reset`)

---

## Step 5-6: Agent + 工具系统 (已完成)

### ReAct Agent
- 多轮工具调用循环（最大 4 轮）
- Session 管理（8 轮历史，自动截断）
- Context Builder（动态系统提示词 + 堆信息）
- 错误恢复（LLM 失败时自动清理历史）

### 20 个内置工具

| 分类 | 工具 | 数量 |
|------|------|------|
| GPIO | `gpio_write`, `gpio_read`, `gpio_read_all`, `delay` | 4 |
| 内存 | `memory_set`, `memory_get`, `memory_delete`, `memory_list` | 4 |
| 定时 | `cron_schedule`, `cron_list`, `cron_cancel`, `cron_cancel_all` | 4 |
| 时间 | `get_time`, `set_timezone` | 2 |
| 系统 | `get_diagnostics`, `get_version` | 2 |
| 人格 | `set_persona`, `get_persona` | 2 |
| 网络 | `wifi_scan`, `get_network_info` | 2 |

---

## Step 7: 多通道系统 (已完成)

| 通道 | 类型 | 说明 | 状态 |
|------|------|------|------|
| Serial | 双向 | UART 串口控制台（默认启用）| ✅ |
| Telegram | 双向 | Bot API 长轮询 + 发送 | ✅ |
| MQTT | 双向 | IoT 标准协议 | ✅ |
| 钉钉 | 单向 | Webhook + HMAC 签名 | ✅ |
| Discord | 单向 | Webhook | ✅ |
| Slack | 单向 | Incoming Webhook | ✅ |
| 企业微信 | 单向 | 群机器人 Markdown | ✅ |
| 飞书 | 单向 | 机器人 + 签名验证 | ✅ |
| Pushplus | 单向 | 统一推送服务 | ✅ |
| Bark | 单向 | iOS 推送通知 | ✅ |

架构特点：
- 条件编译 `CONFIG_ESPCLAW_CHANNEL_xxx`
- 统一接口 `channel_ops_t` vtable
- Serial 消息路由（outbound 按 source 分发到 Serial/Telegram）

---

## Step 8: 定时任务 + 速率限制 + 稳定性 (已完成)

### 8.1 速率限制
- 100 次/小时，1000 次/天
- NVS 持久化日计数
- 超限返回友好提示

### 8.2 定时任务
- 三种类型：periodic / daily / once
- 秒级精度（最小 10 秒）
- NTP 时间同步 + 时区支持
- NVS 持久化（重启恢复）

### 8.3 Telegram 稳定性优化（ESP32-C5 无 PSRAM）
已解决的问题：
- **堆损坏修复**：消除 `free()` 静态缓冲区导致的 MTVAL=0x1b2 崩溃
- **TLS 会话串行化**：全局 TLS 互斥锁，防止 Telegram 轮询与 LLM 同时建立 TLS 连接导致 OOM
- **静态缓冲区**：poll/send/output 全部改用静态分配，消除堆碎片化
- **工具 JSON 修复**：缓冲区从 4KB 扩大到 8KB，20 个工具描述不再截断
- **内存优化**：mbedTLS 动态缓冲 + WiFi 缓冲缩减 + 分区表最大化 factory

运行指标（ESP32-C5, 320KB SRAM, 无 PSRAM）：
- free_heap: ~110KB
- heap_min: ~38KB（Telegram + LLM 并发）
- 固件大小: ~920KB（4MB 分区剩余 78%）

---

## Step 9: 生产就绪 (计划中)

### 9.1 启动保护
- `service/boot_guard.h/.c`
- 记录启动次数到 NVS
- 连续崩溃 >4 次进入安全模式（仅串口，不启动网络）

### 9.2 OTA 升级
- `service/ota.h/.c`
- 双分区 A/B 切换
- HTTPS 固件下载 + 版本验证 + 回滚保护

### 9.3 安全加固
- NVS 加密存储敏感数据
- API key 运行时注入（不硬编码）

---

## Step 10: S3 全功能 (计划中)

### 10.1 文件系统 (S3 only)
- LittleFS: SOUL.md / USER.md / MEMORY.md
- 会话持久化

### 10.2 WebSocket Gateway (S3 only)
- 本地 WebSocket 服务 (port 18789)
- 配合 Web UI

### 10.3 心跳服务 (S3 only)
- 定期检查 HEARTBEAT.md
- 自主执行预设任务

---

## 未来方向 (可选)

| 方向 | 说明 |
|------|------|
| I2C 工具 | 数字传感器 (SHT30/BME280) |
| PWM 工具 | 调光/调速 (LED/电机/舵机) |
| ADC 工具 | 模拟传感器 (光敏/温度/电量) |
| Servo 工具 | 舵机控制 |
| OneWire 工具 | DS18B20 温度传感器 |
| 网络搜索 | Perplexity API / Brave Search |
| 自定义工具 | LLM 通过自然语言创建新工具 |
| 本地 AI | ESP-SR 唤醒词 (S3) |

---

## 竞品对比

| 维度 | ESPClaw | zclaw | mimiclaw | zeroclaw |
|------|---------|-------|----------|----------|
| **定位** | 平衡最优 | 极限体积 | S3 全功能 | 服务器框架 |
| **芯片** | C3/C5/S3 | ESP32 | S3 only | x86/ARM |
| **成本** | $2 | $2 | $5 | $50+ |
| **语言** | C | C | C | Rust |
| **代码量** | 8.5K 行 | ~5K 行 | ~8K 行 | ~15K 行 |
| **工具数** | 20 | 12 | 8 | 75+ |
| **通道数** | 10 | 2 | 2 | 25+ |
| **PSRAM** | 不需要 | 不需要 | 需要 8MB | 需要 512MB+ |
| **GPIO 安全** | ✅ | ✅ | ❌ | ❌ |
| **企业通道** | ✅ | ❌ | ❌ | ✅ |
| **Telegram 双向** | ✅ | ✅ | ✅ | ✅ |
| **定时任务** | ✅ | ✅ | ✅ | ✅ |
| **速率限制** | ✅ | ✅ | ❌ | ✅ |
| **人格切换** | ✅ | ✅ | ❌ | ❌ |
| **OTA** | 📋 | ✅ | ✅ | ❌ |
| **文件系统** | 📋 (S3) | ❌ | ✅ | ✅ |

> **ESPClaw 核心价值**: 在 $2 的 ESP32-C3/C5 上实现完整 AI Agent + 20 个工具 + 10 个通知通道 + 硬件控制，无需 PSRAM。同类项目需要 ESP32-S3 ($5) 或云服务器才能实现同等功能。
