# 机械键盘三模固件

## Goal

RP2350/QMK/Vial 主控通过 SPI 控制 CH592F，实现 USB、BLE、2.4G 三模机械键盘固件。

## Requirements

- 主控固件基于 `vial-kb/vial-qmk`，保持真正 Vial 支持，不把 Vial 核心硬塞进普通 QMK。
- RP2350/QMK/Vial 负责矩阵扫描、keymap、Vial 配置、USB HID，以及向 CH592F 输出无线 HID report。
- CH592F 作为 wireless coprocessor，通过 SPI 接收命令和 HID report，负责 BLE HID 与后续 2.4G transport。
- CH592F 本地工程只作为参考资料；三模主控第一阶段只修改 `vial-qmk`。
- 现有 RP2350、ChibiOS、Vial dirty 改动要纳入 Trellis 任务流，不能作为无来源临时改动继续漂移。

## Acceptance Criteria

- [ ] 父任务包含三模固件目标、阶段拆分和跨阶段约束。
- [ ] 第一阶段子任务独立记录 Vial 识别和 SPI 协议骨架的 PRD、设计与实施计划。
- [ ] 每个后续阶段都能作为可编译、可验收的 Trellis 子任务推进。

## Notes

- 第一阶段子任务：`07-01-rp2350-vial-wc-spi-skeleton`。
- 真实 RP2350B 硬件基线子任务：`07-03-rp2350b-hardware-baseline`。该任务取代前期两键 direct-pin 测试板作为后续开发依据。
