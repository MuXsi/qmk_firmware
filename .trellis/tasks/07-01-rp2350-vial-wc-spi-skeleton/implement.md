# RP2350 Vial 识别 + CH592F SPI 协议骨架 Implementation Plan

## Steps

1. Preserve and verify the existing Pico 2 RP2350 Vial keymap behavior.
2. Add keyboard-local ChibiOS/SPI configuration for SPI0 on `GP17-GP20`.
3. Add a keyboard-local wireless coprocessor module with protocol constants, packed frame type, enums, CRC, frame build/validate helpers, and SPI exchange wrappers.
4. Hook initialization into `keyboard_post_init_kb()` and non-blocking polling into `housekeeping_task_kb()`.
5. Add the module sources and `SPI_DRIVER_REQUIRED = yes` to the keyboard build.
6. Validate Trellis context and run QMK compiles for `vial` and `default`.
7. Inspect build artifacts/descriptors enough to distinguish the new Vial UF2 from older non-Vial firmware.
8. Extend the SPI skeleton into protocol v1 with link state, response validation, retry flagging, event hooks, and command wrappers for transport/pairing/power/time/stats.
9. Add a physical `GP7` long-press BLE pairing trigger using the keyboard-level `process_record_kb()` and housekeeping hooks.
10. Change `wc_exchange()` from same-transaction response matching to a CH592-friendly mailbox sequence: write command, ignore stale RX, wait briefly, send `READ_MAILBOX`, then validate that response.
11. Use the existing frame `flags` field as the response policy switch: `WC_FLAG_RESPONSE_REQUIRED` keeps reliable command behavior, while `wc_command_no_response()` enables future fire-and-forget data frames without changing the wire layout.
12. Make command response matching event-aware: interleaved events are dispatched and skipped, and only a matching RSP can complete a response-required command.
13. Add explicit `WC_STATUS_NO_DATA` mailbox handling so an empty CH592 mailbox is not confused with CRC, SPI, or response-mismatch failure.
14. Bind response-required command completion to `ack_seq == command.seq` so stale responses with the same command id cannot be accepted.
15. Use direct mailbox reads for idle event polling so the ready-state housekeeping path does not create extra response-required commands.
16. Treat `seq=0` as a dummy/empty-mailbox sequence and do not let it replace RP2350's tracked `ack_seq`.
17. Validate frame `type` and zeroed `reserved[2]` fields so malformed frames fail before they reach command or event handling.
18. Limit command retries to transient SPI/framing/stale-response conditions at both exchange and response-validation stages; return deterministic protocol and command failures immediately.
19. Require `wc_command_no_response()` callers to have a ready link so fire-and-forget frames cannot silently succeed while CH592 is offline.
20. Guard HID report packing with a `WC_HID_DATA_MAX` length check before copying report bytes into the protocol payload.
21. Send HID report protocol payloads with `len = 4 + report_len` while preserving the fixed 64-byte SPI frame size.
22. Centralize response policy so control commands remain response-required and `SEND_HID_REPORT` defaults to fire-and-forget unless `WC_HID_REPORT_RESPONSE_REQUIRED` is enabled.
23. Split polling cadence into slow handshake retries and faster ready-state mailbox event polling.
24. Drain a bounded number of ready-state mailbox frames per housekeeping pass so event bursts are handled without unbounded SPI work.
25. Preserve command `seq` across retries while refreshing outgoing `ack_seq` and CRC so retries both identify the original command and acknowledge newer CH592 frames.
26. Treat the full-duplex RX from the command-write phase as stale bus data: acknowledge valid frames, dispatch events, and ignore invalid stale contents.
27. Document little-endian wire encoding for multi-byte fields and assert the RP2350 target endianness because the current implementation uses packed structs as the wire image.
28. Reject inbound `seq=0` frames except for the explicit empty-mailbox `RSP/READ_MAILBOX/NO_DATA` case.
29. Reject invalid frame types during outbound frame construction, not only during inbound validation.
30. Use the same sequence-aware inbound validation for stale RX, mailbox polling, and command response matching.
31. Reject unknown flag bits during outbound frame construction and inbound validation.
32. Enforce flag scope by frame type: `RESPONSE_REQUIRED` and `RETRY` are command-only, while response/event frames may only carry `HAS_MORE`.
33. Reject outbound `seq=0` frames except for the RP2350 `CMD/READ_MAILBOX` dummy read case.
34. Validate public transport arguments locally and reject values outside `USB`, `BLE`, and `24G` before sending SPI frames.
35. Validate `HELLO` response payload compatibility before allowing the link to enter ready state.

## Review Gates

- Do not modify `/home/s/mounriver-studio-projects/CH592F`.
- Do not replace QMK USB HID routing in this task.
- Keep protocol failures isolated from normal USB/Vial keyboard behavior.
- Keep frame size and CRC behavior explicit with compile-time assertions.
- Keep USB/Vial as the active HID path until a later task deliberately adds wireless transport routing.
- The BLE trigger must not suppress normal `GP7` key output; it only adds SPI commands after the hold threshold.
- CH592-side implementation must mirror the mailbox contract; otherwise RP2350 will deliberately reject stale or mismatched responses.
- CH592-side implementation should only enqueue a command response when `WC_FLAG_RESPONSE_REQUIRED` is set; events may still be reported independently through the mailbox.
- CH592-side command responses must copy the handled command sequence into `ack_seq`; RP2350 rejects otherwise valid responses if `ack_seq` does not match the pending command.
- CH592-side implementation may interleave events with responses, but should eventually provide the requested RSP before RP2350's bounded event-read budget is exhausted.
- CH592-side implementation should return `RSP/READ_MAILBOX/NO_DATA` when the mailbox is empty.
- During idle polling, CH592-side implementation should return only queued `EVT` frames or `RSP/READ_MAILBOX/NO_DATA`; a response to an old command is treated as a stale protocol frame.
- CH592-side implementation should use nonzero `seq` values for real responses and events. Empty `NO_DATA` frames may use `seq=0`.
- Fire-and-forget command delivery is not acknowledged by the command path; CH592 should emit completion/drop events for data paths that need observability.
- CH592-side HID report handling should not require an immediate response by default; it should report transmit completion or drops via `WC_EVT_HID_TX_DONE` / `WC_EVT_HID_TX_DROPPED` when that observability is needed.
- CH592-side event mailbox should tolerate regular ready-state polling at the `WC_READY_POLL_INTERVAL_MS` cadence and return `NO_DATA` cheaply when empty.
- CH592-side event mailbox should remain well behaved when RP2350 performs up to `WC_READY_POLL_READS` consecutive mailbox reads in one housekeeping pass.

## Validation Commands

- `python3 ./.trellis/scripts/task.py validate 07-01-rp2350-vial-wc-spi-skeleton`
- `qmk compile -kb handwired/pico2_rp2350_test -km vial`
- `qmk compile -kb handwired/pico2_rp2350_test -km default`

## Rollback

- If SPI enablement breaks RP2350 compilation, revert only the keyboard-local SPI config and module registration.
- If Vial identification regresses, keep the protocol module disabled from the build until the descriptor issue is corrected.
