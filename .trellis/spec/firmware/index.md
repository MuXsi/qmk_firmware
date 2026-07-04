# Firmware Development Guidelines (QMK / Vial)

> Project-specific conventions for `vial-qmk` keyboard firmware on RP2350B tri-mode hardware.

## Documentation Files

| File | Description | When to Read |
| ---- | ----------- | ------------ |
| [rp2350b-hardware-baseline.md](./rp2350b-hardware-baseline.md) | RP2350B pin map, platform bring-up, build targets | Adding keyboards or changing GPIO |
| [wireless-coprocessor-protocol.md](./wireless-coprocessor-protocol.md) | CH592F SPI frame contract and tri-mode HID routing | Changing wireless transport or SPI protocol |

## Core Rules

| Rule | Reference |
| ---- | --------- |
| RP2350 owns transport authority; CH592F only emits HID frames | [wireless-coprocessor-protocol.md](./wireless-coprocessor-protocol.md) |
| Route wireless HID from `host_driver_t` wrapper, not matrix layer | [wireless-coprocessor-protocol.md](./wireless-coprocessor-protocol.md) |
| Raw HID / Vial always pass through USB driver | [wireless-coprocessor-protocol.md](./wireless-coprocessor-protocol.md) |
| Do not enable RGBLIGHT on RP2350 until WS2812 timing is validated | [rp2350b-hardware-baseline.md](./rp2350b-hardware-baseline.md) |
| Matrix issues: try `COL2ROW` before custom matrix code | [rp2350b-hardware-baseline.md](./rp2350b-hardware-baseline.md) |

## Build Commands

```bash
qmk compile -kb handwired/pico2_rp2350_test -km default
qmk compile -kb handwired/pico2_rp2350_test -km vial
```

## Remote

Default push remote: `muxsi` → `https://github.com/MuXsi/qmk_firmware.git` (`master` branch).

---

**Language**: All documentation must be written in **English**.
