// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "tri_mode.h"
#include "wireless_coprocessor.h"
#include "wait.h"

#define BLE_HOLD_ROW 4
#define BLE_HOLD_COL 5

static bool     ble_hold_active;
static bool     ble_hold_sent;
static uint32_t ble_hold_timer;

static bool is_ble_hold_key(keyrecord_t *record) {
    return record->event.key.row == BLE_HOLD_ROW && record->event.key.col == BLE_HOLD_COL;
}

#ifdef RGB_POWER_PIN
static void rgb_power_write(bool enabled) {
    gpio_write_pin(RGB_POWER_PIN, RGB_POWER_ACTIVE_HIGH ? enabled : !enabled);
}

static void rgb_power_init(void) {
    gpio_set_pin_output(RGB_POWER_PIN);
    rgb_power_write(true);
    wait_ms(5);
}
#endif

static void ble_hold_task(void) {
    if (!ble_hold_active || ble_hold_sent || timer_elapsed32(ble_hold_timer) < WC_BLE_HOLD_MS) {
        return;
    }

    ble_hold_sent = true;
    tri_mode_log_status("ble_hold", tri_mode_start_pairing(WC_TRANSPORT_BLE));
}

void keyboard_pre_init_kb(void) {
#ifdef RGB_POWER_PIN
    rgb_power_init();
#endif
    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
    wc_init();
    tri_mode_init();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    wc_task();
    tri_mode_task();
    ble_hold_task();
    housekeeping_task_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (is_ble_hold_key(record)) {
        if (record->event.pressed) {
            ble_hold_active = true;
            ble_hold_sent   = false;
            ble_hold_timer  = timer_read32();
        } else {
            ble_hold_active = false;
        }
    }

    return true;
}

void suspend_power_down_kb(void) {
#ifdef RGB_POWER_PIN
    rgb_power_write(false);
#endif
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
#ifdef RGB_POWER_PIN
    rgb_power_init();
#endif
    suspend_wakeup_init_user();
}
