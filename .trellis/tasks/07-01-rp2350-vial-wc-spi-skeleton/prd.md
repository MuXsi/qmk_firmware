# RP2350 Vial 识别 + CH592F SPI 协议骨架

## Goal

第一阶段：确认 Pico 2 RP2350 两键 Vial 目标可识别，并加入 QMK 侧 CH592F wireless coprocessor SPI 协议骨架。

## Requirements

- 保留 `keyboards/handwired/pico2_rp2350_test` 的两键测试硬件行为：`GP6`、`GP7` 为 direct pins，上拉输入，拉到 GND 表示按下。
- 默认 Vial keymap 继续为 `GP6 = KC_A`、`GP7 = KC_B`，Vial unlock combo 继续为 `GP6 + GP7`。
- Vial 固件必须继续作为真正 Vial 目标编译，设备应暴露 Raw HID interface，供 Vial Web/Desktop 识别。
- RP2350 端新增 CH592F wireless coprocessor SPI 协议骨架，先不替换 QMK USB HID host driver。
- SPI 使用 RP2350 SPI0：`GP17 = CS`、`GP18 = SCK`、`GP19 = MOSI/TX`、`GP20 = MISO/RX`。
- 协议使用新的 fixed 64-byte frame：`WC_MAGIC = 0xA5`、`WC_PROTO_VER = 0x01`、`WC_PAYLOAD_MAX = 48`。
- 协议 `type` 区分 command、response、event；`id` 保存 command id 或 event id。
- CRC 使用 CRC-16/CCITT-FALSE，poly `0x1021`，init `0xFFFF`，覆盖完整 64-byte frame，计算时 `crc16` 字段置零。
- RP2350 侧必须按 CH592 SPI slave 的一帧延迟实现 mailbox 读模型：写命令帧后，再用 dummy read transaction 读取 response。
- CH592F 参考工程中的旧 `'C','W'` 协议只作为参考，不作为本阶段 wire format。
- 没有连接 CH592F 时，SPI/protocol 层失败不能影响 USB/Vial 键盘工作。
- 物理 `GP7` 键长按应作为 BLE pairing 测试触发，不改变短按 `B` 的默认键盘行为。

## Acceptance Criteria

- [x] `qmk compile -kb handwired/pico2_rp2350_test -km vial` 编译通过并生成 UF2。
- [x] `qmk compile -kb handwired/pico2_rp2350_test -km default` 编译通过。
- [x] Vial build 的 USB descriptor 可确认包含 Vial Raw HID interface；若用户设备仍显示 2 个 interface 或旧 `bcdDevice`，判定为未刷入新 UF2。
- [x] `wc_frame_t` 固定为 packed 64 bytes，payload offset 与给定结构兼容，尾部补 `reserved[2]`。
- [x] 协议层提供 frame 构造、CRC、校验、SPI 64-byte exchange、HELLO/PING/GET_STATUS/GET_EVENT/SEND_HID_REPORT/RELEASE_ALL_KEYS API。
- [x] `wc_exchange()` 使用 write-command + read-mailbox 两阶段 SPI transaction，避免把上一帧 response 误判为当前命令 response。
- [x] SPI0/ChibiOS 配置编译进 RP2350 目标，默认 mode 0、divisor 64，并允许通过宏覆盖。
- [x] 代码不修改 `/home/s/mounriver-studio-projects/CH592F`。
- [x] 刷入实物后，Vial Web/Desktop 能识别设备，`GP6` 输出 `A`，`GP7` 输出 `B`，`GP6 + GP7` 可 unlock。
- [x] 长按物理 `GP7` 超过阈值后，RP2350 侧通过 SPI 发送 `SET_TRANSPORT BLE` 和 `START_PAIRING BLE`。

## Notes

- 该子任务只完成第一阶段骨架；完整 BLE/2.4G transport routing、配对 UI、持久化 transport 状态放入后续子任务。
- 2026-07-03 收到 RP2350B QFN-80 真实硬件描述后，`GP6/GP7` 两键 direct-pin 目标仅保留为早期验证记录；后续开发以 `07-03-rp2350b-hardware-baseline` 为硬件基线。
