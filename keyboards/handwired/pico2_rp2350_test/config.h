// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef SPI_DRIVER
#    define SPI_DRIVER SPID0
#endif

#ifndef SPI_SCK_PIN
#    define SPI_SCK_PIN GP34
#endif

#ifndef SPI_MOSI_PIN
#    define SPI_MOSI_PIN GP35
#endif

#ifndef SPI_MISO_PIN
#    define SPI_MISO_PIN GP36
#endif

#ifndef WC_SPI_CS_PIN
#    define WC_SPI_CS_PIN GP33
#endif

#ifndef WC_SPI_MODE
#    define WC_SPI_MODE 0
#endif

#ifndef WC_SPI_DIVISOR
#    define WC_SPI_DIVISOR 16
#endif

#ifndef WC_POLL_INTERVAL_MS
#    define WC_POLL_INTERVAL_MS 1000
#endif

#ifndef WC_HANDSHAKE_POLL_INTERVAL_MS
#    define WC_HANDSHAKE_POLL_INTERVAL_MS WC_POLL_INTERVAL_MS
#endif

#ifndef WC_READY_POLL_INTERVAL_MS
#    define WC_READY_POLL_INTERVAL_MS 10
#endif

#ifndef WC_READY_POLL_READS
#    define WC_READY_POLL_READS 4
#endif

#ifndef WC_COMMAND_RETRIES
#    define WC_COMMAND_RETRIES 2
#endif

#ifndef WC_RESPONSE_EVENT_READS
#    define WC_RESPONSE_EVENT_READS 2
#endif

#ifndef WC_RESPONSE_EMPTY_READS
#    define WC_RESPONSE_EMPTY_READS 2
#endif

#ifndef WC_RESPONSE_DELAY_US
#    define WC_RESPONSE_DELAY_US 50
#endif

#ifndef WC_HID_REPORT_RESPONSE_REQUIRED
#    define WC_HID_REPORT_RESPONSE_REQUIRED 0
#endif

#ifndef WC_BLE_HOLD_MS
#    define WC_BLE_HOLD_MS 1500
#endif

#ifndef WS2812_DI_PIN
#    define WS2812_DI_PIN GP6
#endif

#ifndef RGBLIGHT_LED_COUNT
#    define RGBLIGHT_LED_COUNT 90
#endif

#ifndef RGBLIGHT_LIMIT_VAL
#    define RGBLIGHT_LIMIT_VAL 80
#endif

#ifndef RGBLIGHT_DEFAULT_VAL
#    define RGBLIGHT_DEFAULT_VAL 32
#endif

#ifndef RGBLIGHT_DEFAULT_MODE
#    define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_STATIC_LIGHT
#endif

#ifndef RGB_POWER_PIN
#    define RGB_POWER_PIN GP39
#endif

#ifndef RGB_POWER_ACTIVE_HIGH
#    define RGB_POWER_ACTIVE_HIGH 1
#endif

#ifndef TFT_BL_PIN
#    define TFT_BL_PIN GP24
#endif

#ifndef TFT_CS_PIN
#    define TFT_CS_PIN GP25
#endif

#ifndef TFT_SPI_SCK_PIN
#    define TFT_SPI_SCK_PIN GP26
#endif

#ifndef TFT_SPI_MOSI_PIN
#    define TFT_SPI_MOSI_PIN GP27
#endif

#ifndef TFT_DC_PIN
#    define TFT_DC_PIN GP28
#endif

#ifndef TFT_RST_PIN
#    define TFT_RST_PIN GP29
#endif

#ifndef DEBUG_UART_TX_PIN
#    define DEBUG_UART_TX_PIN GP32
#endif

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

#ifndef VOLUME_ADC_PIN
#    define VOLUME_ADC_PIN GP40
#endif

#ifndef MODE_ADC_PIN
#    define MODE_ADC_PIN GP41
#endif

#ifndef BAT_ADC_PIN
#    define BAT_ADC_PIN GP42
#endif

#ifndef CHRG_PIN
#    define CHRG_PIN GP43
#endif

#ifndef STDBY_PIN
#    define STDBY_PIN GP44
#endif
