// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hal.h"
#include "bootloader.h"
#include "rp_bootrom.h"

#if !defined(RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED)
#    define RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_PIN 0U
#    define RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_ENABLED 0U
#else
#    define RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_PIN RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED
#    define RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_ENABLED 1U
#endif

__attribute__((weak)) void mcu_reset(void) {
    NVIC_SystemReset();
}

void bootloader_jump(void) {
    rpRomResetUsbBoot(RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_PIN, RP2350_BOOTLOADER_DOUBLE_TAP_RESET_LED_ENABLED);
}

void enter_bootloader_mode_if_requested(void) {}
