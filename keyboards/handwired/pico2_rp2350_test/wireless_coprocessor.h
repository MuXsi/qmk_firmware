// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "report.h"

#define WC_MAGIC 0xA5
#define WC_PROTO_VER 0x01
#define WC_FRAME_SIZE 64
#define WC_PAYLOAD_MAX 48
#define WC_HID_DATA_MAX 32

#define WC_FLAG_RESPONSE_REQUIRED 0x01
#define WC_FLAG_ACK_REQUIRED WC_FLAG_RESPONSE_REQUIRED
#define WC_FLAG_HAS_MORE 0x02
#define WC_FLAG_RETRY 0x04
#define WC_FLAG_KNOWN_MASK (WC_FLAG_RESPONSE_REQUIRED | WC_FLAG_HAS_MORE | WC_FLAG_RETRY)

typedef enum {
    WC_FRAME_CMD = 0x01,
    WC_FRAME_RSP = 0x02,
    WC_FRAME_EVT = 0x03,
} wc_frame_type_t;

typedef enum {
    WC_CMD_HELLO             = 0x01,
    WC_CMD_GET_STATUS        = 0x02,
    WC_CMD_GET_EVENT         = 0x03,
    WC_CMD_READ_MAILBOX      = 0x04,
    WC_CMD_PING              = 0x05,

    WC_CMD_SET_TRANSPORT     = 0x10,
    WC_CMD_GET_TRANSPORT     = 0x11,
    WC_CMD_START_PAIRING     = 0x12,
    WC_CMD_STOP_PAIRING      = 0x13,
    WC_CMD_CLEAR_BONDS       = 0x14,

    WC_CMD_SEND_HID_REPORT   = 0x20,
    WC_CMD_RELEASE_ALL_KEYS  = 0x21,

    WC_CMD_SET_BATTERY_LEVEL = 0x30,
    WC_CMD_GET_POWER_STATUS  = 0x31,
    WC_CMD_SLEEP_HINT        = 0x32,

    WC_CMD_TIME_SYNC         = 0x40,
    WC_CMD_GET_TIME          = 0x41,

    WC_CMD_GET_STATS         = 0x70,
    WC_CMD_GET_FW_INFO       = 0x71,
} wc_cmd_id_t;

typedef enum {
    WC_EVT_BOOT              = 0x01,
    WC_EVT_READY             = 0x02,
    WC_EVT_ERROR             = 0x03,

    WC_EVT_BLE_CONNECTED     = 0x10,
    WC_EVT_BLE_DISCONNECTED  = 0x11,
    WC_EVT_BLE_BOND_CHANGED  = 0x12,
    WC_EVT_BLE_PAIRING_START = 0x13,
    WC_EVT_BLE_PAIRING_DONE  = 0x14,

    WC_EVT_24G_CONNECTED     = 0x20,
    WC_EVT_24G_DISCONNECTED  = 0x21,
    WC_EVT_24G_PAIRING_DONE  = 0x22,

    WC_EVT_HID_LED_OUTPUT    = 0x30,
    WC_EVT_HID_TX_DONE       = 0x31,
    WC_EVT_HID_TX_DROPPED    = 0x32,

    WC_EVT_TIME_UPDATED      = 0x41,
} wc_event_id_t;

typedef enum {
    WC_HID_KEYBOARD_6KRO = 0x01,
    WC_HID_CONSUMER     = 0x02,
    WC_HID_MOUSE        = 0x03,
    WC_HID_NKRO         = 0x04,
    WC_HID_VENDOR       = 0x05,
} wc_hid_report_type_t;

typedef enum {
    WC_STATUS_OK                    = 0x0000,
    WC_STATUS_BAD_MAGIC             = 0x0001,
    WC_STATUS_BAD_VERSION           = 0x0002,
    WC_STATUS_BAD_LEN               = 0x0003,
    WC_STATUS_BAD_CRC               = 0x0004,
    WC_STATUS_UNKNOWN_ID            = 0x0005,
    WC_STATUS_BUSY                  = 0x0006,
    WC_STATUS_NOT_READY             = 0x0007,
    WC_STATUS_TIMEOUT               = 0x0008,
    WC_STATUS_TRANSPORT_UNAVAILABLE = 0x0009,
    WC_STATUS_SPI_ERROR             = 0x000A,
    WC_STATUS_BAD_TYPE              = 0x000B,
    WC_STATUS_BAD_RESPONSE          = 0x000C,
    WC_STATUS_EVENT                 = 0x000D,
    WC_STATUS_NO_DATA               = 0x000E,
    WC_STATUS_BAD_RESERVED          = 0x000F,
    WC_STATUS_BAD_SEQ               = 0x0010,
    WC_STATUS_BAD_FLAGS             = 0x0011,
} wc_status_t;

typedef enum {
    WC_TRANSPORT_USB = 0x00,
    WC_TRANSPORT_BLE = 0x01,
    WC_TRANSPORT_24G = 0x02,
} wc_transport_t;

typedef enum {
    WC_LINK_OFFLINE = 0x00,
    WC_LINK_HANDSHAKE,
    WC_LINK_READY,
} wc_link_state_t;

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  version;
    uint16_t seq;
    uint16_t ack_seq;
    uint8_t  type;
    uint8_t  id;
    uint8_t  flags;
    uint8_t  len;
    uint16_t status;
    uint16_t crc16;
    uint8_t  payload[WC_PAYLOAD_MAX];
    uint8_t  reserved[2];
} wc_frame_t;

typedef struct __attribute__((packed)) {
    uint8_t transport;
    uint8_t report_type;
    uint8_t report_id;
    uint8_t report_len;
    uint8_t data[WC_HID_DATA_MAX];
} wc_hid_report_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t proto_version;
    uint8_t frame_size;
    uint8_t payload_max;
    uint8_t reserved;
} wc_hello_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t transport;
} wc_transport_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t level;
} wc_battery_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t unix_time;
} wc_time_sync_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t allow_sleep;
} wc_sleep_hint_payload_t;

void wc_init(void);
void wc_task(void);

bool            wc_is_ready(void);
wc_link_state_t wc_link_state(void);
wc_status_t     wc_last_status(void);
wc_event_id_t   wc_last_event(void);

uint16_t    wc_crc16_ccitt_false(const void *data, uint16_t len);
wc_status_t wc_self_test(void);
wc_status_t wc_build_frame(wc_frame_t *frame, wc_frame_type_t type, uint8_t id, uint16_t seq, uint16_t ack_seq, const void *payload, uint8_t len);
wc_status_t wc_build_frame_with_flags(wc_frame_t *frame, wc_frame_type_t type, uint8_t id, uint8_t flags, uint16_t seq, uint16_t ack_seq, const void *payload, uint8_t len);
wc_status_t wc_validate_frame(const wc_frame_t *frame);
wc_status_t wc_exchange(const wc_frame_t *tx, wc_frame_t *rx);
wc_status_t wc_command(wc_cmd_id_t id, const void *payload, uint8_t len, wc_frame_t *rx);
wc_status_t wc_command_no_response(wc_cmd_id_t id, const void *payload, uint8_t len);

wc_status_t wc_hello(wc_frame_t *rx);
wc_status_t wc_ping(wc_frame_t *rx);
wc_status_t wc_get_status(wc_frame_t *rx);
wc_status_t wc_get_event(wc_frame_t *rx);
wc_status_t wc_set_transport(wc_transport_t transport);
wc_status_t wc_get_transport(wc_frame_t *rx);
wc_status_t wc_start_pairing(wc_transport_t transport);
wc_status_t wc_stop_pairing(wc_transport_t transport);
wc_status_t wc_clear_bonds(wc_transport_t transport);
wc_status_t wc_set_battery_level(uint8_t level);
wc_status_t wc_get_power_status(wc_frame_t *rx);
wc_status_t wc_sleep_hint(bool allow_sleep);
wc_status_t wc_time_sync(uint32_t unix_time);
wc_status_t wc_get_time(wc_frame_t *rx);
wc_status_t wc_get_stats(wc_frame_t *rx);
wc_status_t wc_get_fw_info(wc_frame_t *rx);
wc_status_t wc_send_keyboard_report(wc_transport_t transport, const report_keyboard_t *report);
wc_status_t wc_send_consumer_report(wc_transport_t transport, uint16_t usage);
wc_status_t wc_release_all_keys(wc_transport_t transport);

void wc_event_kb(const wc_frame_t *event);
void wc_event_user(const wc_frame_t *event);
