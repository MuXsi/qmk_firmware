# Implementation Plan

## Steps

- [x] 记录 RP2350B 真实硬件描述，明确旧两键验证板已过期。
- [x] 补齐 RP2350B `GP31-GP47` QMK pin aliases。
- [x] 将 `keyboard.json` 改为 6x15 `ROW2COL` 矩阵。
- [x] 将 default/Vial keymap 扩展到 6x15。
- [x] 将 Vial layout JSON 改为 6x15。
- [x] 将 CH592 SPI 引脚改为 `GP33-GP36`。
- [x] 添加 WS2812/RGB 电源/TFT/ADC/调试 UART 预留宏。
- [x] 暂不启用 RGBLIGHT，避免在未实测 RP2350 WS2812 时序前引入不可靠 bitbang 配置。
- [x] 将长按 B 的 BLE pairing 测试触发更新到新矩阵位置。
- [x] 将长按 B 触发限制为每次按住只尝试一次，避免 CH592F 未 ready 时连续刷屏/重复 release。
- [x] 增加 `tri_mode` 第一版状态骨架和 keyboard-local host-driver wrapper。
- [x] 启用 `GP32` TX-only SIO UART 日志输出。
- [x] 将 CH592F event 回调输出到 `GP32` 日志。
- [x] 增加 `wc_self_test()`，启动时通过 `GP32` 输出 `wc_self=0x0000` 或错误码。
- [x] USB 模式透传 USB HID，BLE/2.4G 模式抑制 USB keyboard/consumer report 并发给 CH592F。
- [x] Raw HID/Vial 始终透传 USB driver。
- [x] BLE/2.4G 模式下 CH592F 未 ready 或发送失败时通过 `GP32` 输出限频日志，避免无线报告静默丢失。
- [x] 增加 report 统计计数，BLE/2.4G 模式下通过 `GP32` 限频输出 `k_tx`、`k_drop`、`c_tx`、`c_drop`、`last_err`。
- [x] 在键盘 README 中记录 `wc_state`、常见 `wc_wait`/send 状态码和上板排查含义。
- [x] 编译 default keymap。
- [x] 编译 Vial keymap。
- [x] 如编译失败，优先处理 RP2350B pin/board/RGBLIGHT 兼容问题。

## Validation

```bash
python3 ./.trellis/scripts/task.py validate 07-03-rp2350b-hardware-baseline
qmk compile -kb handwired/pico2_rp2350_test -km default
qmk compile -kb handwired/pico2_rp2350_test -km vial
```

## Hardware Checks

- USB 模式下矩阵任意键能正常输出。
- 若矩阵无响应，优先把 `ROW2COL` 切到 `COL2ROW` 做方向验证。
- 逻辑分析仪在 `GP33-GP36` 而不是旧 `GP17-GP20` 上观察 CH592 SPI。
- RGB 数据应从 `GP6` 输出，RGB 电源由 `GP39` 控制。
