# Wireless Coprocessor Protocol (CH592F over SPI)

## Scenario: RP2350 ↔ CH592F HID routing

### 1. Scope / Trigger

Apply this spec when changing:

- SPI wiring or CH592F command/event handling
- Tri-mode transport selection (USB / BLE / 2.4G)
- Wireless HID report path from QMK/Vial

RP2350 is the authority for transport and final HID reports. CH592F does not parse QMK keymaps, layers, or Vial config.

### 2. Signatures

Source of truth: `keyboards/handwired/pico2_rp2350_test/wireless_coprocessor.h`

```c
wc_status_t wc_exchange(const wc_frame_t *tx, wc_frame_t *rx);
wc_status_t wc_send_keyboard_report(wc_transport_t transport, const report_keyboard_t *report);
wc_status_t wc_send_consumer_report(wc_transport_t transport, uint16_t usage);
wc_status_t wc_set_transport(wc_transport_t transport);
wc_status_t wc_self_test(void);

void tri_mode_init(void);
wc_status_t tri_mode_set_transport(wc_transport_t transport);
```

Tri-mode wrapper: `keyboards/handwired/pico2_rp2350_test/tri_mode.c`

### 3. Contracts

**SPI pins (RP2350B)**

| Signal | GPIO |
| ------ | ---- |
| CS | GP33 |
| SCK | GP34 |
| MOSI | GP35 |
| MISO | GP36 |

**Frame layout** (`wc_frame_t`, 64 bytes, little-endian)

| Field | Size | Constraint |
| ----- | ---- | ---------- |
| magic | 1 | `0xA5` |
| version | 1 | `0x01` |
| seq / ack_seq | 2 each | non-zero for outbound commands |
| type | 1 | CMD `0x01`, RSP `0x02`, EVT `0x03` |
| id | 1 | command or event id |
| len | 1 | payload ≤ 48 |
| crc16 | 2 | CRC-16/CCITT-FALSE over full frame with crc field zeroed |

**HID report payload** (`wc_hid_report_payload_t`)

| Field | Value |
| ----- | ----- |
| transport | `WC_TRANSPORT_USB/BLE/24G` |
| report_type | `WC_HID_KEYBOARD_6KRO` or `WC_HID_CONSUMER` |
| report_len | ≤ 32 data bytes |

**Tri-mode routing**

| Mode | keyboard/consumer reports | Raw HID |
| ---- | ------------------------- | ------- |
| USB | ChibiOS USB driver | USB driver |
| BLE / 2.4G | `wc_send_*` to CH592F; suppress USB keyboard reports | USB driver (Vial) |

### 4. Validation & Error Matrix

| Condition | Status | Boot log tag |
| --------- | ------ | ------------ |
| Valid frame | `0x0000` OK | `wc_self=0x0000` |
| Bad magic | `0x0001` | `wc_self` or `wc_wait` |
| Bad len | `0x0003` | `wc_wait` |
| Bad CRC | `0x0004` | `wc_wait` |
| CH592F not ready | `0x0007` | `wc_wait`, `drop kb not_ready` |
| SPI failure | `0x000A` | `wc_wait` |
| No mailbox data | `0x000E` | polling only |

Link states: `0x0000` offline, `0x0001` handshake, `0x0002` ready (`wc_state`).

### 5. Good / Base / Bad Cases

- **Good**: USB mode, key press → USB HID report; CH592F receives `SLEEP_HINT`.
- **Base**: BLE mode, CH592F not ready → keyboard report dropped with rate-limited `drop kb not_ready` on GP32.
- **Bad**: Intercept matrix scan and re-encode keys for wireless → breaks Vial dynamic keymap.

### 6. Tests Required

| Test | Assertion |
| ---- | --------- |
| `wc_self_test()` at boot | GP32 prints `wc_self=0x0000` |
| `qmk compile` default + vial | Both succeed |
| USB mode key output | Host receives keystrokes |
| BLE hold on `B` key | One `ble_hold` log per press; no spam while held |
| Wireless mode stats | Every 10s: `k_tx`, `k_drop`, `c_tx`, `c_drop`, `last_err` |

### 7. Wrong vs Correct

#### Wrong

```c
// Re-interpret matrix for wireless inside matrix_scan_kb()
if (wireless_mode) { send_raw_keycode(...); }
```

#### Correct

```c
// tri_mode.c — intercept final HID reports after QMK action layer
static void tri_send_keyboard(report_keyboard_t *report) {
    if (tri_transport == WC_TRANSPORT_USB) {
        usb_host_driver->send_keyboard(report);
        return;
    }
    wc_send_keyboard_report(tri_transport, report);
}
```

## Design Decision: keyboard-local `host_driver_t` wrapper

**Context**: Vial changes must affect USB and wireless equally.

**Decision**: Wrap ChibiOS USB `host_driver_t` in `tri_mode.c` after `keyboard_post_init_kb()`. Raw HID function pointer stays on original USB driver.

**Why not QMK core changes**: Keeps fork diff localized to `handwired/pico2_rp2350_test/` until protocol stabilizes.
