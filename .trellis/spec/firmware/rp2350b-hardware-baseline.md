# RP2350B Hardware Baseline

Keyboard target: `handwired/pico2_rp2350_test` on RP2350B QFN-80.

## Pin Ownership

| GPIO | Function | Notes |
| ---- | -------- | ----- |
| GP0–GP5 | Matrix rows | 6 rows |
| GP8–GP22 | Matrix columns | 15 columns |
| GP6 | WS2812 data | Reserved; RGBLIGHT disabled in v1 |
| GP39 | RGB power control | Active high, default on at boot |
| GP24–GP29 | TFT SPI | Reserved macros only |
| GP33–GP36 | CH592 SPI0 | CS/SCK/MOSI/MISO |
| GP40–GP44 | ADC / charge status | Reserved macros only |
| GP32 | Debug UART TX | 115200 baud, TX-only |

## Matrix

- Size: 6×15 (`ROW2COL` default).
- Use standard QMK matrix scan; do not add custom matrix code for bring-up.
- If keys do not register, switch `diode_direction` to `COL2ROW` in `keyboard.json` before any scanner changes.

## Platform Requirements

- Processor: `RP2350`, bootloader: `rp2350`, board: `GENERIC_RP_RP2350`.
- ChibiOS 9.x submodules required; RP2350 uses `ARMv8-M-ML-ALT` port.
- High GPIO aliases `GP31`–`GP47` live in `platforms/chibios/vendors/RP/_pin_defs.h`.

## RGB (Deferred)

Do **not** enable `RGBLIGHT` until RP2350 WS2812 bit timing is measured on hardware. RP2040 vendor WS2812 driver is not wired for RP2350 yet.

Reserve macros in `config.h`:

- `WS2812_DI_PIN GP6`
- `RGB_POWER_PIN GP39`
- `RGBLIGHT_LED_COUNT 90` (conservative brightness cap)

## BLE Pairing Test Key

- Physical `B` key at matrix row 4, col 5.
- Long-press 1500 ms triggers one BLE pairing attempt per hold (`WC_BLE_HOLD_MS`).
- Do not retry while key remains held if CH592F is not ready.

## Common Mistake: Wrong SPI Analyzer Pins

**Symptom**: `wc_wait=0x0001` (bad magic) or `0x000A` (SPI error).

**Cause**: Logic analyzer still on old `GP17–GP20` from early Pico 2 validation board.

**Fix**: Probe `GP33–GP36`.

## Wrong vs Correct

#### Wrong

```c
// Custom matrix scanner for bring-up
bool matrix_scan_custom(matrix_row_t current_row) { ... }
```

#### Correct

```json
// keyboard.json — flip direction first
"diode_direction": "COL2ROW"
```
