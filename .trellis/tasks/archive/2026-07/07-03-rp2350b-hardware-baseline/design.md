# Design

## Hardware Baseline

RP2350B 作为 QMK/Vial 主控，负责矩阵扫描、USB HID、WS2812 控制和 CH592F SPI 通信。CH592F 仍作为 wireless coprocessor，不理解 QMK keymap，只接收协议帧和 HID report。

## Matrix

使用 QMK 标准矩阵扫描，不写 custom matrix。`ROW2COL` 下 QMK 会逐列拉低并读取行脚，符合当前描述中行脚外部上拉的测试起点。若实测矩阵不响应或出现方向性异常，下一步只切换 `diode_direction`，不改扫描代码。

## Pin Ownership

- `GP0-GP5`：矩阵行。
- `GP8-GP22`：矩阵列。
- `GP6`：WS2812 数据。
- `GP39`：RGB 电源控制。
- `GP24-GP29`：TFT 预留。
- `GP33-GP36`：CH592 SPI。
- `GP40-GP44`：ADC/充电状态预留。
- `GP32`：单向 UART TX 调试预留。

## RGB

本阶段只预留 WS2812/RGBLIGHT 宏和 `GP39` 电源控制，不启用 RGBLIGHT。原因是当前树里的 RP2040 vendor WS2812 driver 没有接入 RP2350，而通用 bitbang driver 需要实测 `WS2812_BITBANG_NOP_FUDGE`。后续应单独做 RP2350 WS2812 驱动验证；拿到灯位图后再升级为 RGB Matrix。

## Wireless SPI

沿用上一阶段 `wireless_coprocessor` 64-byte frame/mailbox 协议，只替换物理 SPI 引脚。第一阶段不修改 QMK core，不替换 Vial/USB 协议栈，而是在键盘目录内通过 host-driver wrapper 接入最终 HID report。

## Tri-mode Authority

RP2350 是三模模式和 HID report 的权威。未来 mode 电路完成后，RP2350 读取 `MODE_ADC` 并决定 USB/BLE/2.4G，再通过 SPI 命令通知 CH592F 当前 wireless transport。CH592F 不做 keymap、层、宏或 Vial 配置判断，只按 RP2350 给出的 HID report 和控制命令发射。

Vial 配置必须影响所有 transport。无线 report routing 从 QMK/Vial 已经解析后的最终 HID report 路径接入，而不是从矩阵扫描层或 raw key event 层重新解释键位。

第一版采用 keyboard-local `host_driver_t` wrapper：`keyboard_post_init_kb()` 后保存 ChibiOS USB driver，替换为 tri-mode wrapper。这样 keyboard/consumer report 来自 QMK action 层之后，天然包含 Vial 动态 keymap、层、tap-hold、combo、key override 等处理结果。USB 模式调用原 driver；BLE/2.4G 模式调用 `wireless_coprocessor` 并不调用 USB keyboard report。Raw HID 始终透传原 USB driver，保证 Vial 在无线模式下仍可访问。

## USB Behavior Across Modes

USB mode 下，QMK 正常通过 USB HID 发送键盘报告，CH592F 进入低功耗但可继续执行 RTC/时间等协处理任务。

BLE/2.4G mode 下，RP2350 仍可枚举 USB 以允许 Vial 访问和配置，但不应通过 USB HID 报告按键。第一版保持完整 USB 配置但在 BLE/2.4G 模式下抑制 keyboard/consumer report，Raw HID/Vial 继续透传 USB。后续可再评估是否构造只暴露 Vial/配置端点的 USB 描述。

## CH592 Hardware Handshake

当前没有 CH592F ready/IRQ、reset 或 power-control 引脚，因此第一版采用 SPI polling/mailbox，并要求没有 CH592F 或 CH592F 不响应时不影响 USB/Vial。后续如果硬件增加 IRQ 和 reset/power-control，应把 polling 降低为 fallback，并加入硬复位/恢复路径。

## Debug UART

`GP32` 只作为 TX-only 日志输出。固件不得依赖 UART RX 或交互式命令；调试开关应通过编译宏、按键或 Vial 自定义功能实现。

## Deferred Work

TFT、ADC 模式识别、电池电压估算、充电状态上报、RGB 持久化和 CH592F 完整 transport UI 都不在本子任务内完成，只保留可编译入口和明确引脚定义。`GP32` TX-only UART 日志和第一版 CH592F report routing 已在本任务内实现。
