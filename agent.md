# Agent 指南：ThreadX_MQTT_H750_Template_clean

## 项目实现目标

- 项目名称：ThreadX_MQTT_H750_Template_clean
- 目标平台：STM32H750，Keil MDK-ARM 工程，基于 STM32CubeMX 生成骨架。
- 核心目标：形成一个可维护的 STM32H750 + ThreadX + ESP8266 MQTT 模板工程，用于通过 MQTT 控制/上报板载或外接 LED 状态，并保留后续扩展到云端 MQTT、Web 控制页、NetX Duo/FileX 的能力。
- 当前主要通信路径：STM32H750 通过 USART1 + DMA 驱动 ESP8266，使用 ESP-AT 指令完成 WiFi 入网和 MQTT 连接。
- 辅助工具目标：`web/led-control.html` 用于浏览器 MQTT 控制；`tools/mqtt-cloud-bridge.mjs` 用于 PC 侧云 MQTT 与本地 MQTT 的桥接测试。

## 协作规则

- 开始修改前先阅读本文件和 `项目记忆.md`，再查看相关源码入口。
- 优先保持改动小而清晰，遵循 CubeMX 的 `USER CODE BEGIN/END` 保护区约定。
- 不要随意改动 `Drivers/`、`Middlewares/ST/`、`MDK-ARM/RTE/` 中的厂商代码；确需修改时必须记录原因、影响范围和验证方式。
- 不要把 WiFi 密码、MQTT 密码、云服务密钥等敏感信息写入文档、提交信息或日志摘要。敏感参数只在本地配置文件或环境变量中维护。
- 遇到用户未提交的源码改动时，默认视为用户工作成果，不要回退、覆盖或格式化无关文件。
- 固件侧修改后，优先验证 Keil 工程能编译；无法本机编译时说明原因，并给出人工验证点。
- PC 工具侧修改后，优先用 `npm`/`node` 做语法或启动检查；不要把 `node_modules/` 当作业务源码阅读或修改。

## 技术约定

- 固件语言：C，少量启动/底层汇编。
- RTOS：Azure RTOS ThreadX。
- 当前业务网络路径：ESP8266 ESP-AT + MQTT。
- 工程文件：`MDK-ARM/ThreadX_MQTT_H750_Template.uvprojx`。
- CubeMX 配置：`ThreadX_MQTT_H750_Template.ioc`。
- PC 工具：Node.js ESM，`package.json` 中 `npm run bridge` 启动 MQTT 桥接脚本。
- 串口分工：USART1 连接 ESP8266，USART3 用作 `printf` 日志输出。
- GPIO 约定：`KEEP_Pin` 为 PB0，`LED_CTRL_Pin` 为 PB1。

## 关键源码入口

- `Core/Src/main.c`：HAL 初始化、时钟、GPIO/I2C/USART 初始化、进入 ThreadX。
- `Core/Src/app_threadx.c`：创建基础 blink 线程；通过 `WIFI_SERVICE_AUTOSTART` 控制是否启动 WiFi/MQTT 服务线程。
- `App/esp8266/esp8266.c`：ESP8266 服务主实现，包含 UART DMA/RingBuffer、AT 事件处理、WiFi/MQTT 状态机和心跳发布。
- `App/esp8266/esp8266_mqtt.c`：ESP-AT MQTT 指令封装。
- `App/esp8266/esp8266_config.h`：WiFi/MQTT、本地缓冲、线程优先级和超时配置。注意其中可能包含敏感配置。
- `Core/Src/stm32h7xx_it.c`：USART1/DMA 中断回调转发到 ESP8266 驱动。
- `web/led-control.html`：浏览器 MQTT 控制页面。
- `tools/mqtt-cloud-bridge.mjs`：PC 侧 MQTT 桥接脚本。

## 重要规则

- STM32H750 是 Cortex-M7，涉及 DMA 缓冲区时必须考虑 DCache、MPU、内存区和 32 字节对齐。
- 如果 DCache 未启用，不要仅凭 `__DCACHE_PRESENT == 1U` 就执行 DCache clean/invalidate；需要确认当前 DCache 实际启用状态。
- DMA 相关缓冲区优先放在 DMA 可访问内存区，并保持合适对齐。
- ThreadX 线程栈、事件标志组、互斥锁等内核对象要有明确生命周期，不要使用临时栈对象保存长期内核资源。
- MQTT 主题、broker、认证配置要区分本地测试与云端部署，不要混用 Web 用户和设备用户权限。
- 修改 `.ioc` 或重新生成 CubeMX 代码后，检查用户代码保护区、Keil 工程包含文件和中断回调是否仍完整。

## 禁止事项

- 不要提交或传播真实 WiFi/MQTT 密码。
- 不要删除 `.git`、`MDK-ARM` 工程文件、CubeMX `.ioc` 或厂商中间件目录。
- 不要把 `node_modules/` 的内容当作项目业务逻辑修改。
- 不要在未确认硬件连接和启动路径前启用多个网络栈路径，避免 ESP8266 路径与 NetX Duo Ethernet 路径混乱。
- 不要在中断回调里执行阻塞操作或复杂解析；中断侧只做最小事件转发。

