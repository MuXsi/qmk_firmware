# RP2350B 真实硬件基线适配

## Goal

将当前 `handwired/pico2_rp2350_test` 从早期 Pico 2 两键验证板，切换为 RP2350B QFN-80 三模机械键盘真实硬件基线，并继续保持 QMK/Vial 可编译。

## Requirements

- 主控为 RP2350B，固件目标继续基于 `vial-qmk`。
- USB 有线模式先保持 QMK/Vial 原生 USB HID 键盘功能。
- 键盘矩阵改为 6 行 15 列，最多 90 键。
- 矩阵行：`ROW0..ROW5 = GP0, GP1, GP2, GP3, GP4, GP5`。
- 矩阵列：`COL0..COL14 = GP8, GP9, GP10, GP11, GP12, GP13, GP14, GP15, GP16, GP17, GP18, GP19, GP20, GP21, GP22`。
- 二极管方向先按 `ROW2COL`，实测异常时再切换到 `COL2ROW` 验证。
- `GP6` 改为 WS2812 数据脚，不再作为按键输入。
- `GP39` 作为 RGB 电源控制脚，默认上电打开；RGB 驱动本阶段先预留，后续在 RP2350 WS2812 时序验证任务中启用。
- CH592F wireless coprocessor SPI 改为：`CS=GP33`、`SCK=GP34`、`MOSI=GP35`、`MISO=GP36`。
- TFT 屏幕本阶段只预留引脚宏：`BL=GP24`、`CS=GP25`、`SCK=GP26`、`MOSI=GP27`、`DC=GP28`、`RST=GP29`。
- ADC/状态输入本阶段只预留引脚宏：`Volume=GP40`、`Mode=GP41`、`Battery=GP42`、`CHRG=GP43`、`STDBY=GP44`。
- RP2350B 调试串口只看到 `U0_TX=GP32`，本阶段只记录/预留，不依赖交互式 RX。
- 长按默认键位中的 `B` 键继续作为 BLE pairing 测试触发。
- 每次 `B` 键长按只触发一次 BLE transport/pairing 尝试，失败后需要松开再重新长按，避免 CH592F 未 ready 时刷屏或反复 release USB report。
- 默认键位按用户提供的 6x15 矩阵表实现：`ROW0 COL13=Insert`、`ROW0 COL14=PrtSc`、`ROW5 COL11=FN`，表中空位使用 `KC_NO`。
- 三模模式权威在 RP2350：未来由 RP2350 读取 `MODE_ADC` 后决定 USB/BLE/2.4G，并通过 SPI 命令告知 CH592F；第一版因模式电路未完成，不添加 mode 按键或 MODE_ADC 切换逻辑。
- CH592F 不理解 QMK keymap、层、宏或 Vial 配置，只作为无线发射协处理器；Vial 改键必须同时影响 USB 和后续 BLE/2.4G 输出。
- USB 模式下 CH592F 应进入低功耗工作状态，但仍可执行时间计算等协处理器任务。
- BLE/2.4G 模式下 RP2350 仍允许 USB 枚举和 Vial 访问，但不应通过 USB HID 发送按键报告；后续可评估只暴露 Vial/配置端点的 USB 描述方式。
- 当前硬件没有 CH592F ready/IRQ 线，也没有 CH592F reset/power control 线；本阶段继续使用 SPI polling/mailbox，后续硬件可新增这些线提升可靠性和功耗表现。
- `GP33-GP36` 在当前硬件设计中确认为 RP2350B `SPI0`。
- RGB 的目标行为是上电恢复用户上次选择的灯效/开关状态；如果用户上次关闭 RGB，则上电保持关闭，状态需要断电保存。第一版暂不启用 RGB 驱动。
- 单向 UART 日志只需要 RP2350 从 `GP32` TX 输出，用户通过外部工具查看日志，不要求 RX 或交互式命令。
- `GP32` 日志需要覆盖 boot、host wrapper ready、transport 切换结果、CH592F ready/连接/配对/HID TX 事件。
- `GP32` 日志需要在启动时输出 RP2350 侧 wireless coprocessor 协议自测结果，用于确认 CRC-16/CCITT-FALSE、64-byte frame 校验和基础错误分类。
- CH592F 未 ready 时，`GP32` 需要限频输出最后一次 wireless coprocessor 状态，方便无 IRQ/ready 线的第一版硬件排查接线和协议问题。
- BLE/2.4G 模式下需要限频输出 keyboard/consumer report 发送与丢弃计数，方便验证 Vial 最终 HID report 是否进入 CH592F 发送路径。
- 第一版使用 keyboard-local `host_driver_t` wrapper 拦截 QMK/Vial 最终 HID report：USB 模式透传原 ChibiOS USB driver；BLE/2.4G 模式把 keyboard/consumer report 发给 CH592F 并抑制 USB 键盘报告；Raw HID/Vial 必须始终透传 USB。
- 第一版不启用 NKRO，无线 report 先覆盖 6KRO keyboard 和 consumer。
- 不修改 CH592F 工程；CH592F 协议仍在 QMK 侧以骨架形式保留。

## Acceptance Criteria

- [x] `keyboard.json` 描述 6x15 矩阵和真实行列引脚。
- [x] default 与 Vial keymap 都能覆盖 6x15 矩阵。
- [x] Vial 描述文件矩阵改为 6x15。
- [x] CH592 SPI 配置改到 `GP33-GP36`。
- [x] WS2812/RGB 相关宏记录 `GP6`、`GP39` 和 90 颗以内的保守亮度限制，但本阶段不启用 RGBLIGHT。
- [x] `GP31-GP47` 在 RP2350 QMK pin defs 中可用，支持 RP2350B 高 GPIO 编译。
- [x] `qmk compile -kb handwired/pico2_rp2350_test -km default` 通过。
- [x] `qmk compile -kb handwired/pico2_rp2350_test -km vial` 通过。
- [x] 第一版模式骨架编译通过，包含 `GP32` TX-only 日志和 host-driver wrapper。
- [x] 启动时输出 `wc_self` 协议自测结果，覆盖有效 frame、坏 magic、坏 len、坏 reserved 和坏 crc。
- [x] CH592F 未 ready 时输出限频 `wc_wait` 日志，ready 状态变化输出 `wc_state`。
- [x] BLE/2.4G 模式下输出限频 `k_tx`、`k_drop`、`c_tx`、`c_drop`、`last_err` report 统计。

## Notes

- 旧的 `GP6/GP7` 两键 direct pins 只属于前期 Vial/RP2350 验证，不再代表当前硬件。
- 真实配列图、RGB LED 坐标/RP2350 WS2812 驱动、ADC 分压校准、TFT 驱动和完整三模路由放到后续子任务。
- 后续新增 mode 电路、CH592F IRQ、CH592F reset/power control 后，需要创建独立 Trellis 子任务并更新 pin ownership。
