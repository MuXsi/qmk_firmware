// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include "wireless_coprocessor.h"

typedef struct {
    uint32_t keyboard_sent;
    uint32_t keyboard_dropped;
    uint32_t consumer_sent;
    uint32_t consumer_dropped;
    wc_status_t last_error;
} tri_report_stats_t;

void tri_mode_init(void);
void tri_mode_task(void);

wc_transport_t tri_mode_transport(void);
wc_status_t    tri_mode_set_transport(wc_transport_t transport);
wc_status_t    tri_mode_start_pairing(wc_transport_t transport);
tri_report_stats_t tri_mode_report_stats(void);

void tri_mode_log(const char *msg);
void tri_mode_log_status(const char *tag, wc_status_t status);
