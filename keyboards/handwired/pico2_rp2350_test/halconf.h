// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef HAL_USE_EFL
#    define HAL_USE_EFL TRUE
#endif

#ifndef HAL_USE_SPI
#    define HAL_USE_SPI TRUE
#endif

#ifndef HAL_USE_SIO
#    define HAL_USE_SIO TRUE
#endif

#include_next <halconf.h>
