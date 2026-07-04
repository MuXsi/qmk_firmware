# RP2350B Tri-mode Keyboard

Handwired RP2350B QFN-80 keyboard target for a USB/BLE/2.4G mechanical keyboard.

## Hardware Baseline

- MCU: RP2350B.
- Matrix: 6 rows x 15 columns, `ROW2COL`.
- Rows: `GP0 GP1 GP2 GP3 GP4 GP5`.
- Columns: `GP8 GP9 GP10 GP11 GP12 GP13 GP14 GP15 GP16 GP17 GP18 GP19 GP20 GP21 GP22`.
- WS2812 data: `GP6`.
- RGB power control: `GP39`.
- CH592F SPI: `CS=GP33`, `SCK=GP34`, `MOSI=GP35`, `MISO=GP36`.
- TFT pins are reserved on `GP24-GP29`.
- ADC/status pins are reserved on `GP40-GP44`.
- Debug UART TX is reserved on `GP32`.

## First Firmware Version

- RP2350 owns the active transport state.
- USB is the default transport.
- Vial Raw HID remains available over USB.
- Keyboard and consumer reports pass through USB in USB mode.
- BLE/2.4G modes suppress USB keyboard reports and send final QMK/Vial reports to CH592F over SPI.
- `GP32` emits TX-only logs at 115200 baud.
- Holding the physical `B` key requests BLE transport and starts BLE pairing.
- MODE_ADC, CH592F IRQ, and CH592F reset/power-control pins are not used in this version.

## Build

Compile the default QMK keymap:

    qmk compile -kb handwired/pico2_rp2350_test -km default

Compile the Vial keymap:

    qmk compile -kb handwired/pico2_rp2350_test -km vial

## Bring-up Checks

Flash `handwired_pico2_rp2350_test_vial.uf2` for Vial testing.

Expected `GP32` TX-only boot logs:

    [tri] boot
    [tri] wc_self=0x0000
    [tri] host wrapper ready
    [tri] wc_state=0x0001

Use a logic analyzer on `GP33-GP36` for CH592F traffic. Until CH592F replies to `HELLO`, the firmware emits a rate-limited `wc_wait=0x....` status. Holding the physical `B` key for 1500 ms attempts BLE transport and pairing once per hold; `ble_hold=0x0000` means the request path succeeded, while `ble_hold=0x0007` means CH592F is not ready yet. If CH592F is not ready while already in wireless mode, keyboard reports log `drop kb not_ready` instead of silently disappearing.

If the matrix does not respond on hardware, first test changing `diode_direction` from `ROW2COL` to `COL2ROW`; do not add a custom matrix scanner for this first pass.

## Wireless Status Hints

`wc_state` values:

- `0x0000`: offline.
- `0x0001`: handshake, waiting for CH592F `HELLO` response.
- `0x0002`: ready.

Common `wc_wait` / send status values:

- `0x0000`: OK.
- `0x0001`: bad magic; likely MISO is floating, frame alignment is wrong, or CH592F is not speaking this protocol.
- `0x0003`: bad length; frame header parsed but `len` is outside the 48-byte payload limit.
- `0x0004`: bad CRC; check byte order, CRC field zeroing, and whether the analyzer is decoding full 64-byte frames.
- `0x0007`: not ready; CH592F has not completed the protocol handshake yet.
- `0x0008`: timeout; SPI transaction failed or CH592F did not clock usable data.
- `0x000A`: SPI error; check `GP33-GP36`, power, ground, and SPI mode 0 wiring.
- `0x000C`: bad response; RP2350 received a valid frame, but it was not the expected response for the command/sequence.
- `0x000E`: no data; mailbox read completed but CH592F had no pending response or event.

In BLE/2.4G mode, report counters are printed every 10 seconds:

- `k_tx`: keyboard reports accepted by the CH592F command path.
- `k_drop`: keyboard reports dropped before or during CH592F send.
- `c_tx`: consumer reports accepted by the CH592F command path.
- `c_drop`: consumer reports dropped before or during CH592F send.
- `last_err`: last wireless report error status.
