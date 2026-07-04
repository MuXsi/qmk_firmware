# RP2350 Vial 识别 + CH592F SPI 协议骨架 Design

## Boundaries

- `vial-qmk` owns the RP2350/QMK/Vial side of the system.
- `/home/s/mounriver-studio-projects/CH592F` is read-only reference material for this task.
- QMK USB and Vial remain the active host-facing transport in this phase.
- The CH592F SPI path is introduced as a callable protocol layer for later BLE/2.4G routing.

## Vial Target

- Keep `handwired/pico2_rp2350_test` as a 1x2 direct-pin board with `GP6` at matrix `[0,0]` and `GP7` at matrix `[0,1]`.
- Keep the Vial keymap at four layers, with layer 0 `KC_A`, `KC_B`, and transparent keys on layers 1-3.
- Keep Vial unlock as both physical keys pressed together.
- Treat a USB descriptor with only two interfaces or old `bcdDevice` as evidence that the board is running an older UF2.

## Wireless Coprocessor Protocol

- Define a new packed `wc_frame_t` in the keyboard target. The frame is exactly 64 bytes:
  - `magic`, `version`, `seq`, `ack_seq`, `type`, `id`, `flags`, `len`, `status`, `crc16`, `payload[48]`, `reserved[2]`.
- Preserve the user-provided field order through `payload[48]`; add only `reserved[2]` at the tail to resolve the 62-byte structure mismatch.
- Multi-byte integer fields are little-endian on the wire. The current RP2350/CH592 path uses the packed struct directly and therefore has a compile-time little-endian target check on the RP2350 side.
- Use `type = 1/2/3` for command/response/event. Command and event enum values follow the product protocol list.
- CRC-16/CCITT-FALSE covers all 64 bytes with the `crc16` field temporarily zeroed. `reserved` bytes are always zero and included in the CRC.
- Encode helpers zero-fill frames, check payload length, assign sequence numbers, and validate magic/version/type/length/reserved/CRC.
- Protocol v1 accepts only known flag bits: `WC_FLAG_RESPONSE_REQUIRED`, `WC_FLAG_HAS_MORE`, and `WC_FLAG_RETRY`. Outbound construction and inbound validation reject unknown bits with `WC_STATUS_BAD_FLAGS`. `WC_FLAG_RESPONSE_REQUIRED` and `WC_FLAG_RETRY` are valid only on command frames; response and event frames may use `WC_FLAG_HAS_MORE` only.
- The protocol layer tracks link state (`offline`, `handshake`, `ready`), last status, last event, and ack sequence.
- Normal command, response, and event frames use nonzero sequence numbers. `seq=0` is reserved for RP2350 `CMD/READ_MAILBOX` dummy frames and may be used by CH592 only for empty-mailbox `RSP/READ_MAILBOX/NO_DATA`; RP2350 rejects other outbound or inbound `seq=0` frames with `WC_STATUS_BAD_SEQ` and does not copy zero into its outgoing `ack_seq`.
- All inbound frame paths that inspect CH592 data use the same sequence-aware validation, including stale full-duplex RX, mailbox polling, and command response matching.
- Commands validate response type, response id, and `ack_seq`. A response-required command only completes when `type=RSP`, `id` equals the command id, and `ack_seq` equals the original command `seq`. Busy or invalid exchanges can be retried with `WC_FLAG_RETRY`; retries keep the original command `seq` and refresh outgoing `ack_seq` before recalculating CRC.
- Retries are limited to transient conditions at both exchange and response-validation stages: busy, SPI timeout/error, bad magic from an unready slave, CRC failure, or stale/mismatched response. Deterministic protocol or command failures such as bad version, bad type, bad length, bad reserved bytes, unknown id, or transport unavailable return immediately.
- Event frames are dispatched through weak `wc_event_kb()` / `wc_event_user()` hooks so later keyboard features can react without changing the protocol core.
- Public command wrappers cover hello, ping, status/event polling, transport switch, pairing, bond clear, battery, sleep hint, time sync, stats, firmware info, HID report, and release-all.
- `HELLO` is the protocol compatibility gate. RP2350 sends its version/frame-size/payload-max tuple and requires the response payload to echo compatible `proto_version`, `frame_size`, and `payload_max` values before the link can enter ready state.
- Public wrappers that carry a transport value validate it locally against `USB`, `BLE`, and `24G`; invalid values return `WC_STATUS_TRANSPORT_UNAVAILABLE` without sending a SPI frame.
- HID report wrappers preserve the QMK report contents, carry the actual HID byte count in `report_len`, set the frame payload `len` to `4 + report_len`, and reject reports larger than `WC_HID_DATA_MAX`. The SPI transaction remains a fixed 64-byte frame; only the protocol payload length changes.
- SPI command exchange uses a mailbox model for CH592 slave timing: the command transaction clocks in and discards the previously prepared frame, waits `WC_RESPONSE_DELAY_US`, then sends `WC_CMD_READ_MAILBOX` (`0x04`) as a no-side-effect dummy frame to clock out the current command response.
- The first transaction's full-duplex RX is treated as a stale frame. If it is valid, RP2350 updates its outgoing `ack_seq`; if it is an event, RP2350 also dispatches it. Invalid stale frames are ignored so a fire-and-forget write is judged by submit status, not by unrelated previous bus contents.
- CH592 firmware must treat `WC_CMD_READ_MAILBOX` as a mailbox read/dummy clock command, not as a normal state-changing command.
- `wc_frame_t.flags` bit 0 is `WC_FLAG_RESPONSE_REQUIRED`. Control commands set it and require CH592 to enqueue a matching response whose `ack_seq` points at the command `seq`; fire-and-forget data frames can clear it and skip the read-mailbox phase.
- Response policy is centralized in the RP2350 protocol layer: control commands require responses by default, while `WC_CMD_SEND_HID_REPORT` uses the fire-and-forget path unless `WC_HID_REPORT_RESPONSE_REQUIRED` is set for debugging.
- Fire-and-forget commands require the RP2350 link state to be ready. They only mean the 64-byte SPI frame was submitted; delivery/completion must be reported later through events if the caller needs confirmation.
- `WC_FLAG_ACK_REQUIRED` remains as an alias of `WC_FLAG_RESPONSE_REQUIRED` for protocol readability while the naming settles.
- If a mailbox read returns an event while a command response is pending, RP2350 handles the event and keeps reading the mailbox a small bounded number of times. An event never counts as a successful command response.
- If a mailbox read has no data, CH592 returns `type=RSP`, `id=WC_CMD_READ_MAILBOX`, `status=WC_STATUS_NO_DATA`. RP2350 treats this as a non-link-breaking empty mailbox and retries only within a bounded budget.
- Idle event polling uses `WC_CMD_READ_MAILBOX` directly. It accepts only `EVT` frames or `RSP/READ_MAILBOX/NO_DATA`; an unsolicited response without a pending command is treated as a protocol mismatch.
- Command-level statuses such as busy or transport unavailable update `wc_last_status()` but do not drop the SPI link; framing, CRC, timeout, or mismatched response errors do.

## RP2350 SPI

- Use QMK's `spi_master` API and enable `SPI_DRIVER_REQUIRED`.
- Enable ChibiOS SPI0 with keyboard-local `mcuconf.h` and `halconf.h` changes instead of changing CH592F sources.
- Use macros for pins and timing:
  - `WC_SPI_CS_PIN GP17`
  - `SPI_DRIVER SPID0`
  - `SPI_SCK_PIN GP18`
  - `SPI_MOSI_PIN GP19`
  - `SPI_MISO_PIN GP20`
  - `WC_SPI_MODE 0`
  - `WC_SPI_DIVISOR 16`
- Use fixed 64-byte full-duplex exchange. If CH592F is absent or returns invalid data, report timeout/not-ready style status and keep USB/Vial operational.
- `WC_RESPONSE_DELAY_US` defaults to 50 us and can be tuned after logic-analyzer or CH592 firmware timing measurements.
- `WC_HANDSHAKE_POLL_INTERVAL_MS` controls slow HELLO retries while CH592 is absent or not ready; it defaults to the legacy `WC_POLL_INTERVAL_MS` value.
- `WC_READY_POLL_INTERVAL_MS` controls mailbox event polling after the link is ready and defaults to 10 ms so BLE/HID completion events are not delayed by the slower handshake cadence.
- `WC_READY_POLL_READS` bounds how many mailbox frames RP2350 will drain per ready-state housekeeping pass. It defaults to 4 and stops early on `NO_DATA`, keeping event bursts responsive without allowing unbounded SPI work.
- `WC_RESPONSE_EVENT_READS` bounds how many interleaved events RP2350 will consume while waiting for a command response. This keeps the protocol simple and prevents blocking forever if CH592 never provides the expected response.
- `WC_RESPONSE_EMPTY_READS` bounds how many empty mailbox reads RP2350 will tolerate while waiting for a response.
- The default SPI divisor is 16 to keep the two-frame mailbox command path comfortably below a 1 ms budget on RP-class clocks. If wiring or CH592 timing is unstable, tune the divisor upward.
- For 1 kHz HID traffic, HID report writes avoid a strict command-response wait by default. Completion/drop status should be delivered later through events such as `WC_EVT_HID_TX_DONE` and `WC_EVT_HID_TX_DROPPED`.

## Hooks

- Initialize the protocol/SPI layer from `keyboard_post_init_kb()`.
- Poll lightweight CH592F events from `housekeeping_task_kb()` without blocking key scanning. The ready-state mailbox cadence is separate from the slower handshake retry cadence, and each ready pass drains only a bounded number of frames.
- Do not intercept QMK's USB HID report path in this phase. Provide HID report send APIs for future transport routing.
- During this phase, `wc_task()` handshakes with `HELLO` and reads the mailbox once the link is ready. If CH592F is absent or returns invalid frames, the link returns to handshake/offline without affecting USB/Vial.
- Physical `GP7` long-press is a board-level BLE pairing trigger. It leaves normal QMK/Vial key processing intact, waits for `wc_is_ready()`, then sends `SET_TRANSPORT BLE` followed by `START_PAIRING BLE`.
