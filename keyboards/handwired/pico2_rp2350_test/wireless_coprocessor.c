// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wireless_coprocessor.h"

#include <stddef.h>
#include <string.h>
#include "config.h"
#include "spi_master.h"
#include "timer.h"
#include "wait.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#    error "wireless coprocessor protocol currently expects a little-endian target"
#endif

_Static_assert(sizeof(wc_frame_t) == WC_FRAME_SIZE, "wc_frame_t must be 64 bytes");
_Static_assert(offsetof(wc_frame_t, crc16) == 12, "wc_frame_t crc16 offset changed");
_Static_assert(offsetof(wc_frame_t, payload) == 14, "wc_frame_t payload offset changed");
_Static_assert(offsetof(wc_hid_report_payload_t, data) == 4, "wc_hid_report_payload_t header must be 4 bytes");
_Static_assert(sizeof(wc_hid_report_payload_t) <= WC_PAYLOAD_MAX, "HID payload must fit in wc_frame_t");

static bool     wc_initialized;
static wc_link_state_t wc_state = WC_LINK_OFFLINE;
static uint16_t wc_next_seq = 1;
static uint16_t wc_ack_seq;
static uint32_t wc_last_poll;
static wc_status_t   wc_error = WC_STATUS_NOT_READY;
static wc_event_id_t wc_event;

static uint16_t wc_calculate_frame_crc(const wc_frame_t *frame) {
    wc_frame_t copy = *frame;
    copy.crc16     = 0;
    return wc_crc16_ccitt_false(&copy, sizeof(copy));
}

static wc_status_t wc_finalize_frame(wc_frame_t *frame) {
    if (!frame || frame->len > WC_PAYLOAD_MAX) {
        return WC_STATUS_BAD_LEN;
    }

    frame->crc16 = 0;
    frame->crc16 = wc_crc16_ccitt_false(frame, sizeof(*frame));
    return WC_STATUS_OK;
}

static void wc_update_ack(const wc_frame_t *frame) {
    if (frame && frame->seq != 0 && wc_validate_frame(frame) == WC_STATUS_OK) {
        wc_ack_seq = frame->seq;
    }
}

static uint16_t wc_allocate_seq(void) {
    uint16_t seq = wc_next_seq++;

    if (wc_next_seq == 0) {
        wc_next_seq = 1;
    }

    return seq;
}

static uint8_t wc_hid_payload_len(const wc_hid_report_payload_t *payload) {
    return (uint8_t)(offsetof(wc_hid_report_payload_t, data) + payload->report_len);
}

static wc_status_t wc_validate_flags(wc_frame_type_t type, uint8_t flags) {
    if ((flags & ~WC_FLAG_KNOWN_MASK) != 0) {
        return WC_STATUS_BAD_FLAGS;
    }

    if (type != WC_FRAME_CMD && (flags & (WC_FLAG_RESPONSE_REQUIRED | WC_FLAG_RETRY)) != 0) {
        return WC_STATUS_BAD_FLAGS;
    }

    return WC_STATUS_OK;
}

static wc_status_t wc_validate_outbound_seq(wc_frame_type_t type, uint8_t id, uint16_t seq) {
    if (seq != 0) {
        return WC_STATUS_OK;
    }

    if (type == WC_FRAME_CMD && id == WC_CMD_READ_MAILBOX) {
        return WC_STATUS_OK;
    }

    return WC_STATUS_BAD_SEQ;
}

static wc_status_t wc_validate_transport(wc_transport_t transport) {
    switch (transport) {
        case WC_TRANSPORT_USB:
        case WC_TRANSPORT_BLE:
        case WC_TRANSPORT_24G:
            return WC_STATUS_OK;
        default:
            return WC_STATUS_TRANSPORT_UNAVAILABLE;
    }
}

static wc_status_t wc_validate_hello_response(const wc_frame_t *rx) {
    if (!rx || rx->len < sizeof(wc_hello_payload_t)) {
        return WC_STATUS_BAD_LEN;
    }

    wc_hello_payload_t hello;
    memcpy(&hello, rx->payload, sizeof(hello));

    if (hello.proto_version != WC_PROTO_VER || hello.frame_size != WC_FRAME_SIZE || hello.payload_max != WC_PAYLOAD_MAX) {
        return WC_STATUS_BAD_RESPONSE;
    }

    return WC_STATUS_OK;
}

static bool wc_command_requires_response(wc_cmd_id_t id) {
    switch (id) {
        case WC_CMD_SEND_HID_REPORT:
            return WC_HID_REPORT_RESPONSE_REQUIRED != 0;
        default:
            return true;
    }
}

static bool wc_status_breaks_link(wc_status_t status) {
    switch (status) {
        case WC_STATUS_OK:
        case WC_STATUS_BUSY:
        case WC_STATUS_UNKNOWN_ID:
        case WC_STATUS_TRANSPORT_UNAVAILABLE:
        case WC_STATUS_EVENT:
        case WC_STATUS_NO_DATA:
            return false;
        default:
            return true;
    }
}

static bool wc_status_can_retry(wc_status_t status) {
    switch (status) {
        case WC_STATUS_BUSY:
        case WC_STATUS_TIMEOUT:
        case WC_STATUS_SPI_ERROR:
        case WC_STATUS_BAD_MAGIC:
        case WC_STATUS_BAD_CRC:
        case WC_STATUS_BAD_RESPONSE:
            return true;
        default:
            return false;
    }
}

static void wc_set_error(wc_status_t status) {
    wc_error = status;
    if (wc_status_breaks_link(status)) {
        wc_state = WC_LINK_OFFLINE;
    }
}

static void wc_handle_event(const wc_frame_t *event) {
    if (!event || event->type != WC_FRAME_EVT) {
        return;
    }

    wc_event = (wc_event_id_t)event->id;
    if (event->id == WC_EVT_READY) {
        wc_state = WC_LINK_READY;
    }

    wc_event_kb(event);
}

static bool wc_is_empty_mailbox_frame(const wc_frame_t *frame) {
    return frame && frame->type == WC_FRAME_RSP && frame->id == WC_CMD_READ_MAILBOX && frame->status == WC_STATUS_NO_DATA;
}

static wc_status_t wc_validate_inbound_frame(const wc_frame_t *frame) {
    wc_status_t status = wc_validate_frame(frame);
    if (status != WC_STATUS_OK) {
        return status;
    }

    if (frame->seq == 0 && !wc_is_empty_mailbox_frame(frame)) {
        return WC_STATUS_BAD_SEQ;
    }

    return WC_STATUS_OK;
}

static void wc_handle_stale_frame(const wc_frame_t *frame) {
    if (wc_validate_inbound_frame(frame) != WC_STATUS_OK) {
        return;
    }

    wc_update_ack(frame);
    if (frame->type == WC_FRAME_EVT) {
        wc_handle_event(frame);
    }
}

static wc_status_t wc_response_status(wc_cmd_id_t id, uint16_t expected_ack_seq, const wc_frame_t *rx) {
    wc_status_t status = wc_validate_inbound_frame(rx);
    if (status != WC_STATUS_OK) {
        return status;
    }

    if (rx->type == WC_FRAME_EVT) {
        wc_handle_event(rx);
        return WC_STATUS_EVENT;
    }

    if (rx->type != WC_FRAME_RSP) {
        return WC_STATUS_BAD_TYPE;
    }

    if (rx->id == WC_CMD_READ_MAILBOX && rx->status == WC_STATUS_NO_DATA) {
        return WC_STATUS_NO_DATA;
    }

    if (rx->id != id) {
        return WC_STATUS_BAD_RESPONSE;
    }

    if (rx->ack_seq != expected_ack_seq) {
        return WC_STATUS_BAD_RESPONSE;
    }

    return (wc_status_t)rx->status;
}

static wc_status_t wc_transceive_frame(const wc_frame_t *tx, wc_frame_t *rx) {
    if (!spi_start(WC_SPI_CS_PIN, false, WC_SPI_MODE, WC_SPI_DIVISOR)) {
        return WC_STATUS_SPI_ERROR;
    }

    const uint8_t *tx_bytes = (const uint8_t *)tx;
    uint8_t       *rx_bytes = (uint8_t *)rx;
    for (uint8_t i = 0; i < WC_FRAME_SIZE; i++) {
        spi_status_t status = spi_write(tx_bytes[i]);
        if (status < 0) {
            spi_stop();
            return WC_STATUS_TIMEOUT;
        }
        rx_bytes[i] = (uint8_t)status;
    }

    spi_stop();
    return WC_STATUS_OK;
}

static wc_status_t wc_read_mailbox(wc_frame_t *rx) {
    wc_frame_t  mailbox_read;
    wc_status_t status = wc_build_frame(&mailbox_read, WC_FRAME_CMD, WC_CMD_READ_MAILBOX, 0, wc_ack_seq, NULL, 0);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_transceive_frame(&mailbox_read, rx);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_validate_inbound_frame(rx);
    if (status != WC_STATUS_OK) {
        return status;
    }

    wc_update_ack(rx);
    return WC_STATUS_OK;
}

static wc_status_t wc_poll_mailbox(wc_frame_t *rx) {
    wc_status_t status = wc_read_mailbox(rx);
    if (status != WC_STATUS_OK) {
        return status;
    }

    if (rx->type == WC_FRAME_EVT) {
        wc_handle_event(rx);
        return WC_STATUS_EVENT;
    }

    if (rx->type == WC_FRAME_RSP && rx->id == WC_CMD_READ_MAILBOX && rx->status == WC_STATUS_NO_DATA) {
        return WC_STATUS_NO_DATA;
    }

    return WC_STATUS_BAD_RESPONSE;
}

static uint32_t wc_poll_interval(void) {
    return wc_state == WC_LINK_READY ? WC_READY_POLL_INTERVAL_MS : WC_HANDSHAKE_POLL_INTERVAL_MS;
}

static wc_status_t wc_drain_mailbox(void) {
    wc_frame_t   rx;
    wc_status_t status = WC_STATUS_NO_DATA;

    for (uint8_t i = 0; i < WC_READY_POLL_READS; i++) {
        status = wc_poll_mailbox(&rx);
        if (status == WC_STATUS_NO_DATA) {
            return WC_STATUS_NO_DATA;
        }
        if (status != WC_STATUS_EVENT) {
            return status;
        }
    }

    return WC_STATUS_EVENT;
}

void wc_init(void) {
    spi_init();
    wc_initialized = true;
    wc_state       = WC_LINK_HANDSHAKE;
    wc_error       = WC_STATUS_NOT_READY;
    wc_last_poll   = timer_read32();
}

void wc_task(void) {
    if (!wc_initialized || timer_elapsed32(wc_last_poll) < wc_poll_interval()) {
        return;
    }

    wc_last_poll = timer_read32();

    wc_frame_t rx;
    if (wc_state != WC_LINK_READY) {
        wc_state = wc_hello(&rx) == WC_STATUS_OK ? WC_LINK_READY : WC_LINK_HANDSHAKE;
        return;
    }

    wc_status_t status = wc_drain_mailbox();
    if (status != WC_STATUS_EVENT && status != WC_STATUS_NO_DATA) {
        wc_set_error(status);
    }
}

uint16_t wc_crc16_ccitt_false(const void *data, uint16_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t       crc   = 0xFFFF;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000) != 0) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

bool wc_is_ready(void) {
    return wc_state == WC_LINK_READY;
}

wc_link_state_t wc_link_state(void) {
    return wc_state;
}

wc_status_t wc_last_status(void) {
    return wc_error;
}

wc_event_id_t wc_last_event(void) {
    return wc_event;
}

wc_status_t wc_build_frame_with_flags(wc_frame_t *frame, wc_frame_type_t type, uint8_t id, uint8_t flags, uint16_t seq, uint16_t ack_seq, const void *payload, uint8_t len) {
    if (!frame || len > WC_PAYLOAD_MAX || (len > 0 && !payload)) {
        return WC_STATUS_BAD_LEN;
    }

    if (type != WC_FRAME_CMD && type != WC_FRAME_RSP && type != WC_FRAME_EVT) {
        return WC_STATUS_BAD_TYPE;
    }

    wc_status_t status = wc_validate_flags(type, flags);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_validate_outbound_seq(type, id, seq);
    if (status != WC_STATUS_OK) {
        return status;
    }

    memset(frame, 0, sizeof(*frame));
    frame->magic   = WC_MAGIC;
    frame->version = WC_PROTO_VER;
    frame->seq     = seq;
    frame->ack_seq = ack_seq;
    frame->type    = type;
    frame->id      = id;
    frame->flags   = flags;
    frame->len     = len;
    frame->status  = WC_STATUS_OK;

    if (len > 0) {
        memcpy(frame->payload, payload, len);
    }

    return wc_finalize_frame(frame);
}

wc_status_t wc_build_frame(wc_frame_t *frame, wc_frame_type_t type, uint8_t id, uint16_t seq, uint16_t ack_seq, const void *payload, uint8_t len) {
    return wc_build_frame_with_flags(frame, type, id, 0, seq, ack_seq, payload, len);
}

wc_status_t wc_self_test(void) {
    static const uint8_t crc_test[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    wc_frame_t          frame;
    wc_status_t         status;

    if (wc_crc16_ccitt_false(crc_test, sizeof(crc_test)) != 0x29B1) {
        return WC_STATUS_BAD_CRC;
    }

    status = wc_build_frame(&frame, WC_FRAME_CMD, WC_CMD_PING, 1, 0, NULL, 0);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_validate_frame(&frame);
    if (status != WC_STATUS_OK) {
        return status;
    }

    frame.magic = 0;
    status      = wc_validate_frame(&frame);
    if (status != WC_STATUS_BAD_MAGIC) {
        return WC_STATUS_BAD_RESPONSE;
    }

    frame.magic = WC_MAGIC;
    frame.len   = WC_PAYLOAD_MAX + 1;
    status      = wc_validate_frame(&frame);
    if (status != WC_STATUS_BAD_LEN) {
        return WC_STATUS_BAD_RESPONSE;
    }

    frame.len = 0;
    frame.reserved[0] = 1;
    status = wc_validate_frame(&frame);
    if (status != WC_STATUS_BAD_RESERVED) {
        return WC_STATUS_BAD_RESPONSE;
    }

    frame.reserved[0] = 0;
    frame.crc16 ^= 0xFFFF;
    status = wc_validate_frame(&frame);
    if (status != WC_STATUS_BAD_CRC) {
        return WC_STATUS_BAD_RESPONSE;
    }

    return WC_STATUS_OK;
}

wc_status_t wc_validate_frame(const wc_frame_t *frame) {
    if (!frame) {
        return WC_STATUS_NOT_READY;
    }

    if (frame->magic != WC_MAGIC) {
        return WC_STATUS_BAD_MAGIC;
    }

    if (frame->version != WC_PROTO_VER) {
        return WC_STATUS_BAD_VERSION;
    }

    if (frame->type != WC_FRAME_CMD && frame->type != WC_FRAME_RSP && frame->type != WC_FRAME_EVT) {
        return WC_STATUS_BAD_TYPE;
    }

    wc_status_t status = wc_validate_flags((wc_frame_type_t)frame->type, frame->flags);
    if (status != WC_STATUS_OK) {
        return status;
    }

    if (frame->len > WC_PAYLOAD_MAX) {
        return WC_STATUS_BAD_LEN;
    }

    if (frame->reserved[0] != 0 || frame->reserved[1] != 0) {
        return WC_STATUS_BAD_RESERVED;
    }

    if (frame->crc16 != wc_calculate_frame_crc(frame)) {
        return WC_STATUS_BAD_CRC;
    }

    return WC_STATUS_OK;
}

wc_status_t wc_exchange(const wc_frame_t *tx, wc_frame_t *rx) {
    if (!wc_initialized || !tx || ((tx->flags & WC_FLAG_RESPONSE_REQUIRED) && !rx)) {
        return WC_STATUS_NOT_READY;
    }

    wc_frame_t stale_rx;
    wc_status_t status = wc_transceive_frame(tx, &stale_rx);
    if (status != WC_STATUS_OK) {
        return status;
    }

    wc_handle_stale_frame(&stale_rx);

    if ((tx->flags & WC_FLAG_RESPONSE_REQUIRED) == 0) {
        if (rx) {
            *rx = stale_rx;
        }
        return WC_STATUS_OK;
    }

    if (WC_RESPONSE_DELAY_US > 0) {
        wait_us(WC_RESPONSE_DELAY_US);
    }

    return wc_read_mailbox(rx);
}

wc_status_t wc_command(wc_cmd_id_t id, const void *payload, uint8_t len, wc_frame_t *rx) {
    wc_frame_t   tx;
    wc_frame_t   local_rx;
    wc_frame_t  *rx_frame = rx ? rx : &local_rx;
    wc_status_t  status   = wc_build_frame_with_flags(&tx, WC_FRAME_CMD, id, WC_FLAG_RESPONSE_REQUIRED, wc_allocate_seq(), wc_ack_seq, payload, len);
    const uint16_t expected_ack_seq = tx.seq;
    if (status != WC_STATUS_OK) {
        wc_set_error(status);
        return status;
    }

    for (uint8_t attempt = 0; attempt <= WC_COMMAND_RETRIES; attempt++) {
        if (attempt > 0) {
            tx.ack_seq = wc_ack_seq;
            tx.flags |= WC_FLAG_RETRY;
            status = wc_finalize_frame(&tx);
            if (status != WC_STATUS_OK) {
                wc_set_error(status);
                return status;
            }
        }

        status = wc_exchange(&tx, rx_frame);
        if (status != WC_STATUS_OK) {
            if (!wc_status_can_retry(status)) {
                break;
            }
            continue;
        }

        status = wc_response_status(id, expected_ack_seq, rx_frame);
        if (status == WC_STATUS_OK) {
            wc_error = WC_STATUS_OK;
            return WC_STATUS_OK;
        }

        uint8_t event_reads = 0;
        uint8_t empty_reads = 0;
        while ((status == WC_STATUS_EVENT && event_reads < WC_RESPONSE_EVENT_READS) || (status == WC_STATUS_NO_DATA && empty_reads < WC_RESPONSE_EMPTY_READS)) {
            if (status == WC_STATUS_EVENT) {
                event_reads++;
            } else {
                empty_reads++;
            }

            status = wc_read_mailbox(rx_frame);
            if (status == WC_STATUS_OK) {
                status = wc_response_status(id, expected_ack_seq, rx_frame);
            }

            if (status == WC_STATUS_OK) {
                wc_error = WC_STATUS_OK;
                return WC_STATUS_OK;
            }
        }

        if (status == WC_STATUS_EVENT || status == WC_STATUS_NO_DATA) {
            status = WC_STATUS_BAD_RESPONSE;
        }

        if (!wc_status_can_retry(status)) {
            break;
        }
    }

    wc_set_error(status);
    return status;
}

wc_status_t wc_command_no_response(wc_cmd_id_t id, const void *payload, uint8_t len) {
    if (wc_state != WC_LINK_READY) {
        wc_set_error(WC_STATUS_NOT_READY);
        return WC_STATUS_NOT_READY;
    }

    wc_frame_t  tx;
    wc_status_t status = wc_build_frame(&tx, WC_FRAME_CMD, id, wc_allocate_seq(), wc_ack_seq, payload, len);
    if (status != WC_STATUS_OK) {
        wc_set_error(status);
        return status;
    }

    status = wc_exchange(&tx, NULL);
    if (status != WC_STATUS_OK) {
        wc_set_error(status);
        return status;
    }

    wc_error = WC_STATUS_OK;
    return WC_STATUS_OK;
}

static wc_status_t wc_command_by_policy(wc_cmd_id_t id, const void *payload, uint8_t len, wc_frame_t *rx) {
    if (wc_command_requires_response(id)) {
        return wc_command(id, payload, len, rx);
    }

    if (rx) {
        return WC_STATUS_BAD_RESPONSE;
    }

    return wc_command_no_response(id, payload, len);
}

wc_status_t wc_hello(wc_frame_t *rx) {
    wc_frame_t local_rx;
    wc_frame_t *rx_frame = rx ? rx : &local_rx;
    const wc_hello_payload_t payload = {
        .proto_version = WC_PROTO_VER,
        .frame_size    = WC_FRAME_SIZE,
        .payload_max   = WC_PAYLOAD_MAX,
    };

    wc_status_t status = wc_command(WC_CMD_HELLO, &payload, sizeof(payload), rx_frame);
    if (status != WC_STATUS_OK) {
        return status;
    }

    status = wc_validate_hello_response(rx_frame);
    if (status != WC_STATUS_OK) {
        wc_set_error(status);
        return status;
    }

    return WC_STATUS_OK;
}

wc_status_t wc_ping(wc_frame_t *rx) {
    return wc_command(WC_CMD_PING, NULL, 0, rx);
}

wc_status_t wc_get_status(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_STATUS, NULL, 0, rx);
}

wc_status_t wc_get_event(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_EVENT, NULL, 0, rx);
}

wc_status_t wc_set_transport(wc_transport_t transport) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    const wc_transport_payload_t payload = {.transport = transport};
    return wc_command(WC_CMD_SET_TRANSPORT, &payload, sizeof(payload), NULL);
}

wc_status_t wc_get_transport(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_TRANSPORT, NULL, 0, rx);
}

wc_status_t wc_start_pairing(wc_transport_t transport) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    const wc_transport_payload_t payload = {.transport = transport};
    return wc_command(WC_CMD_START_PAIRING, &payload, sizeof(payload), NULL);
}

wc_status_t wc_stop_pairing(wc_transport_t transport) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    const wc_transport_payload_t payload = {.transport = transport};
    return wc_command(WC_CMD_STOP_PAIRING, &payload, sizeof(payload), NULL);
}

wc_status_t wc_clear_bonds(wc_transport_t transport) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    const wc_transport_payload_t payload = {.transport = transport};
    return wc_command(WC_CMD_CLEAR_BONDS, &payload, sizeof(payload), NULL);
}

wc_status_t wc_set_battery_level(uint8_t level) {
    const wc_battery_payload_t payload = {.level = level};
    return wc_command(WC_CMD_SET_BATTERY_LEVEL, &payload, sizeof(payload), NULL);
}

wc_status_t wc_get_power_status(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_POWER_STATUS, NULL, 0, rx);
}

wc_status_t wc_sleep_hint(bool allow_sleep) {
    const wc_sleep_hint_payload_t payload = {.allow_sleep = allow_sleep ? 1 : 0};
    return wc_command(WC_CMD_SLEEP_HINT, &payload, sizeof(payload), NULL);
}

wc_status_t wc_time_sync(uint32_t unix_time) {
    const wc_time_sync_payload_t payload = {.unix_time = unix_time};
    return wc_command(WC_CMD_TIME_SYNC, &payload, sizeof(payload), NULL);
}

wc_status_t wc_get_time(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_TIME, NULL, 0, rx);
}

wc_status_t wc_get_stats(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_STATS, NULL, 0, rx);
}

wc_status_t wc_get_fw_info(wc_frame_t *rx) {
    return wc_command(WC_CMD_GET_FW_INFO, NULL, 0, rx);
}

wc_status_t wc_send_keyboard_report(wc_transport_t transport, const report_keyboard_t *report) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    if (!report) {
        return WC_STATUS_NOT_READY;
    }

    wc_hid_report_payload_t payload = {
        .transport   = transport,
        .report_type = WC_HID_KEYBOARD_6KRO,
    };

#ifdef KEYBOARD_SHARED_EP
    payload.report_id  = report->report_id;
    payload.report_len = sizeof(*report) - 1;
    if (payload.report_len > WC_HID_DATA_MAX) {
        return WC_STATUS_BAD_LEN;
    }
    memcpy(payload.data, ((const uint8_t *)report) + 1, payload.report_len);
#else
    payload.report_id  = REPORT_ID_KEYBOARD;
    payload.report_len = sizeof(*report);
    if (payload.report_len > WC_HID_DATA_MAX) {
        return WC_STATUS_BAD_LEN;
    }
    memcpy(payload.data, report, payload.report_len);
#endif

    return wc_command_by_policy(WC_CMD_SEND_HID_REPORT, &payload, wc_hid_payload_len(&payload), NULL);
}

wc_status_t wc_send_consumer_report(wc_transport_t transport, uint16_t usage) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    wc_hid_report_payload_t payload = {
        .transport   = transport,
        .report_type = WC_HID_CONSUMER,
        .report_id   = REPORT_ID_CONSUMER,
        .report_len  = sizeof(usage),
        .data         = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)},
    };

    return wc_command_by_policy(WC_CMD_SEND_HID_REPORT, &payload, wc_hid_payload_len(&payload), NULL);
}

wc_status_t wc_release_all_keys(wc_transport_t transport) {
    wc_status_t status = wc_validate_transport(transport);
    if (status != WC_STATUS_OK) {
        return status;
    }

    uint8_t payload = transport;
    return wc_command(WC_CMD_RELEASE_ALL_KEYS, &payload, sizeof(payload), NULL);
}

__attribute__((weak)) void wc_event_kb(const wc_frame_t *event) {
    wc_event_user(event);
}

__attribute__((weak)) void wc_event_user(const wc_frame_t *event) {
    (void)event;
}
