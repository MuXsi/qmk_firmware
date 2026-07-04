// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tri_mode.h"

#include <string.h>
#include "config.h"
#include "gpio.h"
#include "host.h"
#include "host_driver.h"
#include "report.h"
#include "timer.h"

#ifndef TRI_MODE_DEBUG_ENABLE
#    define TRI_MODE_DEBUG_ENABLE 1
#endif

#ifndef TRI_MODE_DEBUG_BAUD
#    define TRI_MODE_DEBUG_BAUD 115200
#endif

#ifndef TRI_MODE_WC_STATUS_LOG_MS
#    define TRI_MODE_WC_STATUS_LOG_MS 5000
#endif

#ifndef TRI_MODE_REPORT_STATS_LOG_MS
#    define TRI_MODE_REPORT_STATS_LOG_MS 10000
#endif

#if TRI_MODE_DEBUG_ENABLE
#    include <hal.h>
#endif

static wc_transport_t tri_transport = WC_TRANSPORT_USB;
static bool           usb_sleep_hint_sent;
static uint32_t       kb_drop_log_timer;
static uint32_t       consumer_drop_log_timer;
static uint32_t       wireless_error_log_timer;
static uint32_t       wc_status_log_timer;
static uint32_t       report_stats_log_timer;
static wc_link_state_t last_wc_state = 0xFF;
static tri_report_stats_t report_stats;

static host_driver_t *usb_host_driver;
static bool           host_driver_wrapped;

static uint8_t tri_keyboard_leds(void);
static void    tri_send_keyboard(report_keyboard_t *report);
static void    tri_send_nkro(report_nkro_t *report);
static void    tri_send_mouse(report_mouse_t *report);
static void    tri_send_extra(report_extra_t *report);

static host_driver_t tri_host_driver = {
    .keyboard_leds = tri_keyboard_leds,
    .send_keyboard = tri_send_keyboard,
    .send_nkro     = tri_send_nkro,
    .send_mouse    = tri_send_mouse,
    .send_extra    = tri_send_extra,
#ifdef RAW_ENABLE
    .send_raw_hid = NULL,
#endif
};

#if TRI_MODE_DEBUG_ENABLE
static bool tri_debug_ready;

static SIOConfig tri_debug_sio_config = {
    .baud      = TRI_MODE_DEBUG_BAUD,
    .UARTLCR_H = (UART_UARTLCR_H_WLEN_8BITS | UART_UARTLCR_H_FEN),
    .UARTCR    = 0U,
    .UARTIFLS  = (UART_UARTIFLS_RXIFLSEL_1_8F | UART_UARTIFLS_TXIFLSEL_1_8E),
    .UARTDMACR = 0U,
};

static void tri_debug_init(void) {
    if (tri_debug_ready) {
        return;
    }

    palSetLineMode(DEBUG_UART_TX_PIN, PAL_MODE_ALTERNATE_UART);
    sioStart(&SIOD0, &tri_debug_sio_config);
    tri_debug_ready = true;
}

static void tri_debug_write(const char *msg) {
    if (!tri_debug_ready || !msg) {
        return;
    }

    chnWrite(&SIOD0, (const uint8_t *)msg, strlen(msg));
}
#else
static void tri_debug_init(void) {}
static void tri_debug_write(const char *msg) {
    (void)msg;
}
#endif

void tri_mode_log(const char *msg) {
    tri_debug_write("[tri] ");
    tri_debug_write(msg);
    tri_debug_write("\r\n");
}

void tri_mode_log_status(const char *tag, wc_status_t status) {
    static const char hex[] = "0123456789ABCDEF";
    char              line[24];
    uint16_t          value = (uint16_t)status;
    uint8_t           i     = 0;

    while (tag && *tag && i < sizeof(line) - 9) {
        line[i++] = *tag++;
    }

    line[i++] = '=';
    line[i++] = '0';
    line[i++] = 'x';
    line[i++] = hex[(value >> 12) & 0xF];
    line[i++] = hex[(value >> 8) & 0xF];
    line[i++] = hex[(value >> 4) & 0xF];
    line[i++] = hex[value & 0xF];
    line[i]   = '\0';

    tri_mode_log(line);
}

static void tri_mode_log_limited(const char *msg, uint32_t *timer) {
    if (!timer) {
        return;
    }

    if (*timer != 0 && timer_elapsed32(*timer) < 1000) {
        return;
    }

    *timer = timer_read32();
    tri_mode_log(msg);
}

static void tri_mode_log_status_limited(const char *tag, wc_status_t status, uint32_t *timer) {
    if (!timer) {
        return;
    }

    if (*timer != 0 && timer_elapsed32(*timer) < 1000) {
        return;
    }

    *timer = timer_read32();
    tri_mode_log_status(tag, status);
}

static void tri_wrap_host_driver(void) {
    host_driver_t *current = host_get_driver();

    if (!current || current == &tri_host_driver || host_driver_wrapped) {
        return;
    }

    usb_host_driver = current;
#ifdef RAW_ENABLE
    tri_host_driver.send_raw_hid = current->send_raw_hid;
#endif
    host_set_driver(&tri_host_driver);
    host_driver_wrapped = true;
    tri_mode_log("host wrapper ready");
}

static uint8_t tri_keyboard_leds(void) {
    if (!usb_host_driver || !usb_host_driver->keyboard_leds) {
        return 0;
    }

    return usb_host_driver->keyboard_leds();
}

static void tri_send_usb_release(void) {
    if (!usb_host_driver) {
        return;
    }

    if (usb_host_driver->send_keyboard) {
        report_keyboard_t report = {0};
        usb_host_driver->send_keyboard(&report);
    }

    if (usb_host_driver->send_extra) {
        report_extra_t report = {
            .report_id = REPORT_ID_CONSUMER,
            .usage     = 0,
        };
        usb_host_driver->send_extra(&report);
    }
}

static void tri_send_keyboard(report_keyboard_t *report) {
    if (tri_transport == WC_TRANSPORT_USB) {
        if (usb_host_driver && usb_host_driver->send_keyboard) {
            usb_host_driver->send_keyboard(report);
        }
        return;
    }

    if (wc_is_ready()) {
        wc_status_t status = wc_send_keyboard_report(tri_transport, report);
        if (status != WC_STATUS_OK) {
            report_stats.keyboard_dropped++;
            report_stats.last_error = status;
            tri_mode_log_status_limited("kb_send", status, &wireless_error_log_timer);
        } else {
            report_stats.keyboard_sent++;
        }
    } else {
        report_stats.keyboard_dropped++;
        report_stats.last_error = WC_STATUS_NOT_READY;
        tri_mode_log_limited("drop kb not_ready", &kb_drop_log_timer);
    }
}

static void tri_send_nkro(report_nkro_t *report) {
    if (tri_transport == WC_TRANSPORT_USB && usb_host_driver && usb_host_driver->send_nkro) {
        usb_host_driver->send_nkro(report);
    }
}

static void tri_send_mouse(report_mouse_t *report) {
    if (tri_transport == WC_TRANSPORT_USB && usb_host_driver && usb_host_driver->send_mouse) {
        usb_host_driver->send_mouse(report);
    }
}

static void tri_send_extra(report_extra_t *report) {
    if (tri_transport == WC_TRANSPORT_USB) {
        if (usb_host_driver && usb_host_driver->send_extra) {
            usb_host_driver->send_extra(report);
        }
        return;
    }

    if (wc_is_ready() && report && report->report_id == REPORT_ID_CONSUMER) {
        wc_status_t status = wc_send_consumer_report(tri_transport, report->usage);
        if (status != WC_STATUS_OK) {
            report_stats.consumer_dropped++;
            report_stats.last_error = status;
            tri_mode_log_status_limited("consumer_send", status, &wireless_error_log_timer);
        } else {
            report_stats.consumer_sent++;
        }
    } else if (report && report->report_id == REPORT_ID_CONSUMER) {
        report_stats.consumer_dropped++;
        report_stats.last_error = WC_STATUS_NOT_READY;
        tri_mode_log_limited("drop consumer not_ready", &consumer_drop_log_timer);
    }
}

static wc_status_t tri_apply_ch592_transport(wc_transport_t transport) {
    wc_status_t status;

    if (!wc_is_ready()) {
        return WC_STATUS_NOT_READY;
    }

    status = wc_set_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    return wc_sleep_hint(transport == WC_TRANSPORT_USB);
}

static void tri_log_wc_status(void) {
    wc_link_state_t state = wc_link_state();

    if (state != last_wc_state) {
        last_wc_state = state;
        tri_mode_log_status("wc_state", state);
    }

    if (!wc_is_ready() && (wc_status_log_timer == 0 || timer_elapsed32(wc_status_log_timer) >= TRI_MODE_WC_STATUS_LOG_MS)) {
        wc_status_log_timer = timer_read32();
        tri_mode_log_status("wc_wait", wc_last_status());
    }
}

static void tri_log_report_stats(void) {
    if (tri_transport == WC_TRANSPORT_USB) {
        return;
    }

    if (report_stats_log_timer != 0 && timer_elapsed32(report_stats_log_timer) < TRI_MODE_REPORT_STATS_LOG_MS) {
        return;
    }

    report_stats_log_timer = timer_read32();
    tri_mode_log_status("k_tx", (wc_status_t)(report_stats.keyboard_sent & 0xFFFF));
    tri_mode_log_status("k_drop", (wc_status_t)(report_stats.keyboard_dropped & 0xFFFF));
    tri_mode_log_status("c_tx", (wc_status_t)(report_stats.consumer_sent & 0xFFFF));
    tri_mode_log_status("c_drop", (wc_status_t)(report_stats.consumer_dropped & 0xFFFF));
    tri_mode_log_status("last_err", report_stats.last_error);
}

void tri_mode_init(void) {
    tri_debug_init();
    tri_mode_log("boot");
    tri_mode_log_status("wc_self", wc_self_test());
    tri_wrap_host_driver();
}

void tri_mode_task(void) {
    tri_wrap_host_driver();
    tri_log_wc_status();
    tri_log_report_stats();

    if (tri_transport == WC_TRANSPORT_USB && wc_is_ready() && !usb_sleep_hint_sent) {
        wc_status_t status = tri_apply_ch592_transport(WC_TRANSPORT_USB);
        usb_sleep_hint_sent = status == WC_STATUS_OK;
        tri_mode_log_status("usb_sleep", status);
    }
}

wc_transport_t tri_mode_transport(void) {
    return tri_transport;
}

tri_report_stats_t tri_mode_report_stats(void) {
    return report_stats;
}

wc_status_t tri_mode_set_transport(wc_transport_t transport) {
    wc_status_t status;

    if (transport == tri_transport) {
        return WC_STATUS_OK;
    }

    if (transport == WC_TRANSPORT_USB) {
        if (tri_transport != WC_TRANSPORT_USB && wc_is_ready()) {
            wc_release_all_keys(tri_transport);
        }

        tri_transport       = WC_TRANSPORT_USB;
        status              = tri_apply_ch592_transport(WC_TRANSPORT_USB);
        usb_sleep_hint_sent = status == WC_STATUS_OK;
        tri_mode_log_status("mode_usb", status);
        return WC_STATUS_OK;
    } else {
        tri_send_usb_release();
    }

    status = tri_apply_ch592_transport(transport);
    if (status != WC_STATUS_OK) {
        tri_mode_log_status("mode_fail", status);
        return status;
    }

    tri_transport = transport;
    usb_sleep_hint_sent = transport == WC_TRANSPORT_USB;
    tri_mode_log_status("mode", status);
    return WC_STATUS_OK;
}

wc_status_t tri_mode_start_pairing(wc_transport_t transport) {
    wc_status_t status = tri_mode_set_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_start_pairing(transport);
    tri_mode_log_status("pairing", status);
    return status;
}

void wc_event_kb(const wc_frame_t *event) {
    if (!event) {
        return;
    }

    switch (event->id) {
        case WC_EVT_BOOT:
            tri_mode_log("evt boot");
            break;
        case WC_EVT_READY:
            tri_mode_log("evt ready");
            break;
        case WC_EVT_ERROR:
            tri_mode_log("evt error");
            break;
        case WC_EVT_BLE_CONNECTED:
            tri_mode_log("evt ble connected");
            break;
        case WC_EVT_BLE_DISCONNECTED:
            tri_mode_log("evt ble disconnected");
            break;
        case WC_EVT_BLE_PAIRING_START:
            tri_mode_log("evt ble pairing start");
            break;
        case WC_EVT_BLE_PAIRING_DONE:
            tri_mode_log("evt ble pairing done");
            break;
        case WC_EVT_24G_CONNECTED:
            tri_mode_log("evt 24g connected");
            break;
        case WC_EVT_24G_DISCONNECTED:
            tri_mode_log("evt 24g disconnected");
            break;
        case WC_EVT_24G_PAIRING_DONE:
            tri_mode_log("evt 24g pairing done");
            break;
        case WC_EVT_HID_TX_DONE:
            tri_mode_log("evt hid tx done");
            break;
        case WC_EVT_HID_TX_DROPPED:
            tri_mode_log("evt hid tx dropped");
            break;
        default:
            tri_mode_log_status("evt", event->id);
            break;
    }

    wc_event_user(event);
}
