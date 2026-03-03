# ESPClaw 增量开发计划

## 项目定位

ESP-IDF 5.5 纯 C AI 助手固件，主攻 ESP32-C3，兼容 ESP32-S3。

## 核心规则

- **每一步都必须能编译 + 烧录 + 验证**
- 新模块逐个加入 CMakeLists.txt，不提前引用不存在的文件
- C3 优先，S3 特性用 `#if ESPCLAW_HAS_xxx` 条件编译

---

## 当前进度

- [x] Step 1 — 最小可编译固件 (已验证烧录)
- [x] Step 2 — WiFi 连接 (已验证，注意 WPA 版本兼容性)
- [x] Step 3 — 串口交互 (已验证，C5 上运行正常)
- [x] Step 4 — LLM 对话 (已验证，支持 Anthropic / OpenAI 兼容接口)
- [ ] Step 5 — ReAct Agent ← 当前
- [ ] Step 6 — 工具系统
- [ ] Step 7 — Telegram
- [ ] Step 8 — 生产就绪
- [ ] Step 9 — S3 全功能
- [ ] Step 10 — 完整功能集

---

## Step 1: 最小可编译固件 ✅

**新增文件:** main.c, platform.h, config.h, CMakeLists.txt, sdkconfig.*, partitions_*.csv, idf_component.yml
**CMakeLists SRCS:** `main.c`
**功能:** 打印 banner → NVS init → 打印堆内存大小
**验证:**
```bash
idf.py set-target esp32c3 && idf.py build
# 串口输出: "ESPClaw v0.1.0 / Target: esp32c3 / Free heap: xxxxx bytes"
```
**通过标准:** 编译成功，串口看到 banner 和堆内存数值

---

## Step 2: WiFi 连接

**新增文件:**
- `mem/nvs_manager.h` + `.c` — NVS 读写封装
- `net/wifi_manager.h` + `.c` — WiFi STA 连接
- `nvs_keys.h` — NVS 键名常量（已存在）

**CMakeLists SRCS 追加:** `mem/nvs_manager.c`, `net/wifi_manager.c`
**main.c 改动:** 调用 nvs_mgr_init() → wifi_mgr_init_and_connect()

**功能:**
1. 从 NVS 读取 WiFi SSID/密码（fallback 到 Kconfig 编译时值）
2. 连接 WiFi，打印 IP 地址

**验证:**
```bash
idf.py menuconfig  # 填入 WiFi SSID + 密码
idf.py build flash monitor
# 串口输出: "WiFi connected, IP: 192.168.x.x"
```
**通过标准:** 设备成功获取 IP 地址

---

## Step 3: 串口交互

**新增文件:**
- `bus/message_bus.h` + `.c` — FreeRTOS 队列消息总线
- `channel/channel.h` — Channel vtable 接口定义
- `channel/channel_serial.c` — 串口输入/输出 channel
- `channel/channel_registry.c` — channel 枚举启动
- `messages.h` — 消息类型定义（已存在）
- `util/text_buffer.h` + `.c` — 文本缓冲工具

**CMakeLists SRCS 追加:** 以上 4 个 .c 文件

**功能:**
1. 串口输入一行文本 → 进入 inbound 队列
2. agent task 从队列取出 → 暂时原样 echo 回去 → 写入 outbound 队列
3. 串口输出 task 从 outbound 队列取出 → 打印到串口

**验证:**
```
# 串口输入: hello
# 串口输出: [echo] hello
```
**通过标准:** 输入任意文本，串口回显，证明消息总线双向通畅

---

## Step 4: LLM 对话

**新增文件:**
- `provider/provider.h` — Provider vtable 接口
- `provider/provider_anthropic.c` — Anthropic Messages API
- `provider/provider_openai.c` — OpenAI 兼容 API（暂时 stub）
- `provider/provider_ollama.c` — Ollama（暂时 stub）
- `provider/provider_registry.c` — backend 选择
- `provider/llm_auth.h` + `.c` — API key 管理
- `util/json_util.h` + `.c` — cJSON 辅助
- `util/json_stream.h` + `.c` — SAX 流式 JSON 解析器

**CMakeLists SRCS 追加:** 以上 .c 文件
**CMakeLists REQUIRES 追加:** `esp_http_client`, `esp_tls`, `mbedtls`, `json`

**功能:**
1. 从 NVS/Kconfig 读取 API key + model
2. 将串口输入作为 user message，构建 Anthropic 请求 JSON
3. HTTPS POST 到 Anthropic API
4. 解析响应，提取 text 内容
5. 串口输出 LLM 回复

**验证:**
```
# 串口输入: what is 2+2?
# 串口输出: [claude] 2+2 equals 4.
```
**通过标准:** 真实 LLM 回复出现在串口

---

## Step 5: ReAct Agent

**新增文件:**
- `agent/agent_loop.h` + `.c` — ReAct 循环主逻辑
- `agent/context_builder.h` + `.c` — 系统提示词组装
- `agent/session.h` + `.c` — 对话历史管理

**CMakeLists SRCS 追加:** 以上 3 个 .c 文件

**功能:**
1. 维护对话历史（内存数组，MAX_HISTORY_TURNS 轮）
2. 构建系统提示词（静态 prompt + 设备信息）
3. ReAct 循环：检测 tool_use → 执行 → 返回结果 → 再次调用 LLM
4. 多轮对话上下文保持

**验证:**
```
# 串口输入: hi
# 串口输出: Hello! How can I help?
# 串口输入: what did I just say?
# 串口输出: You said "hi". （证明历史生效）
```
**通过标准:** 多轮对话有上下文记忆

---

## Step 6: 工具系统

**新增文件:**
- `tool/tool.h` — Tool 接口 + 能力位掩码
- `tool/tool_registry.h` + `.c` — 工具注册 + 分发
- `tool/builtin_tools.def` — X-macro 工具表
- `tool/tool_gpio.c` — GPIO 读写工具
- `tool/tool_memory.c` — NVS 持久记忆工具
- `tool/tool_system.c` — 系统诊断工具
- `hal/hal_gpio.h` + `.c` — GPIO HAL vtable + 安全护栏

**CMakeLists SRCS 追加:** 以上 .c 文件
**CMakeLists REQUIRES 追加:** `driver`

**功能:**
1. 工具自动注册，按平台能力过滤
2. LLM 可调用 gpio_write/read 控制引脚
3. LLM 可调用 memory_set/get 持久存储
4. LLM 可调用 get_diagnostics 查看设备状态
5. GPIO 安全护栏：只允许 Kconfig 配置范围内的引脚

**验证:**
```
# 串口输入: turn on GPIO 2
# 实际效果: GPIO2 变高电平（万用表量）
# 串口输出: Done, GPIO 2 set to HIGH.
# 串口输入: remember my name is Alex
# 串口输入: (重启后) what is my name?
# 串口输出: Your name is Alex. （NVS 持久化验证）
```
**通过标准:** LLM 成功调用工具并返回结果；GPIO 实际改变电平

---

## Step 7: Telegram

**新增文件:**
- `channel/channel_telegram.h` + `.c` — Telegram bot 主逻辑
- `channel/channel_telegram_poll.c` — 长轮询
- `channel/channel_telegram_token.c` — token 管理
- `channel/channel_telegram_chat_ids.c` — chat ID 白名单
- `security/security.h` + `.c` — 安全检查

**CMakeLists SRCS 追加:** 以上 .c 文件

**功能:**
1. 从 NVS 读取 Telegram bot token + 允许的 chat ID
2. HTTPS 长轮询 getUpdates
3. 解析消息 → 送入 inbound 队列 → agent 处理 → outbound → 回复到 Telegram
4. 未授权 chat ID 拒绝

**验证:**
```
# 手机 Telegram 发送: /start
# 收到回复: Hello! I'm ESPClaw.
# 发送: what GPIO pins can you control?
# 收到回复: I can control GPIO pins 2-10...
```
**通过标准:** 手机上完成一次完整对话

---

## Step 8: 生产就绪

**新增文件:**
- `provider/ratelimit.h` + `.c` — 速率限制
- `service/cron_service.h` + `.c` — 定时任务
- `service/cron_utils.h` + `.c` — 时间辅助
- `service/boot_guard.h` + `.c` — 启动保护
- `service/ota.h` + `.c` — OTA 升级
- `tool/tool_cron.c` — 定时任务工具

**CMakeLists SRCS 追加:** 以上 .c 文件
**CMakeLists REQUIRES 追加:** `esp_https_ota`

**功能:**
1. LLM API 速率限制（100/小时，1000/天）
2. Cron 定时任务（daily/periodic/once），NVS 持久化
3. 启动保护（连续崩溃 >4 次进安全模式）
4. OTA 固件升级（双分区 A/B）

**验证:**
```
# Telegram: remind me in 1 minute to drink water
# 1 分钟后自动收到提醒
# idf.py size → 确认固件 < 888KB
```
**通过标准:** 定时任务工作 + 固件 < 888KB + 启动保护不误触

---

## Step 9: S3 全功能适配

**新增文件:**
- `mem/storage.h` + `.c` — LittleFS 管理
- `channel/channel_ws.h` + `.c` — WebSocket gateway
- `hal/hal_spi.h` + `.c` — SPI HAL
- `service/heartbeat.h` + `.c` — 心跳自主任务
- `tool/tool_files.c` — 文件读写工具
- `proxy/http_proxy.h` + `.c` — HTTP 代理

**CMakeLists 改动:** `if(CONFIG_IDF_TARGET_ESP32S3)` 追加 S3-only 源文件

**功能:**
1. 双核调度：Core 0 = I/O，Core 1 = Agent
2. PSRAM 大缓冲区（32KB request/response）
3. LittleFS 存储 SOUL.md/USER.md/MEMORY.md/session
4. WebSocket 本地网关（port 18789）
5. 心跳服务（30 分钟检查 HEARTBEAT.md）

**验证:**
```bash
idf.py set-target esp32s3 && idf.py build flash monitor
# 验证: 双核 task 分布、WebSocket 连接、文件持久化
```
**通过标准:** S3 上所有 C3 功能正常 + WebSocket 可连接 + 文件持久化

---

## Step 10: 完整功能集

**新增文件:**
- `hal/hal_i2c.h` + `.c`, `hal/hal_pwm.h` + `.c`
- `tool/tool_i2c.c`, `tool/tool_persona.c`, `tool/tool_user.h` + `.c`

**功能:**
1. I2C 扫描/读写工具
2. PWM 控制工具
3. 人格切换（neutral/friendly/technical/witty）
4. 用户自定义工具（LLM 通过自然语言创建工具）

**通过标准:** LLM 可扫描 I2C 设备 + 切换人格 + 自创工具

---

## 架构总览

```
串口/Telegram/WebSocket
       │
  ┌────▼────┐
  │ Channel │ ← vtable: init/start/write/is_available
  └────┬────┘
       │ inbound_msg_t
  ┌────▼────────┐
  │ Message Bus │ FreeRTOS Queue (深度 8)
  └────┬────────┘
       │
  ┌────▼──────┐
  │ Agent     │ ReAct Loop (C3 单核 / S3 Core1)
  │ Loop      │ ← context_builder + session
  └────┬──────┘
       │
  ┌────▼──────────┐
  │ Provider      │ ← vtable: build_request/request/parse_response
  │ (Anthropic/   │   C3: 流式 JSON 解析
  │  OpenAI/...)  │   S3: cJSON 全量解析
  └───────────────┘
       │
  ┌────▼──────────┐
  │ Tool Registry │ ← 能力位掩码自动过滤
  │ gpio/memory/  │   C3: BASIC+GPIO+I2C
  │ cron/system/..│   S3: +LITTLEFS+SPI+...
  └───────────────┘
       │
  ┌────▼────┐
  │  HAL    │ ← vtable: 函数指针表
  │ GPIO/I2C│   安全护栏在此层拦截
  └─────────┘
```

## 关键文件索引

| 文件 | 作用 | 引入步骤 |
|------|------|---------|
| platform.h | C3/S3 条件编译宏 | Step 1 |
| config.h | 所有编译时常量 | Step 1 |
| messages.h | 队列消息类型 | Step 3 |
| nvs_keys.h | NVS 键名 | Step 2 |
| provider.h | LLM Provider vtable | Step 4 |
| channel.h | Channel vtable | Step 3 |
| tool.h | Tool 接口 + 能力掩码 | Step 6 |
| hal_gpio.h | GPIO HAL vtable | Step 6 |

## 参考源码

- zclaw agent 循环: `../zclaw/main/agent.c`
- zclaw 工具注册: `../zclaw/main/tools.c` + `builtin_tools.def`
- zclaw 配置: `../zclaw/main/config.h`
- mimiclaw 双核: `../mimiclaw/main/mimi.c`
- mimiclaw 消息总线: `../mimiclaw/main/bus/`
- zeroclaw Trait 模式: `../zeroclaw/src/providers/traits.rs`
