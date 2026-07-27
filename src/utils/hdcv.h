/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_UTILS_HDCV_H
#define SRC_UTILS_HDCV_H

#include "macros.h"

/*
 * Hubble Device Configuration Vector (HDCV) — a compact, fixed-format string
 * describing this build's crypto, EID-rotation and network configuration.
 * It is derived entirely from build options so the device can report or log
 * its configuration.
 *
 * HDCV:<version>/E:<enc>[/CS:<src>][/EC:<count>][/RP:<period>][/N:<net>][/TV:<ver>][/SV:<ver>]
 */

/* E — encryption / key size */
#if defined(CONFIG_HUBBLE_NETWORK_KEY_256)
#define HDCV_E "/E:256"
#elif defined(CONFIG_HUBBLE_NETWORK_KEY_128)
#define HDCV_E "/E:128"
#else
#define HDCV_E ""
#endif

/* CS — counter source  */
#if defined(CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME)
#define HDCV_CS "/CS:DU"
#elif defined(CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME)
#define HDCV_CS "/CS:UT"
#else
#define HDCV_CS ""
#endif

/* EC — EID count; only meaningful for device uptime, fixed at 128 today */
#if defined(CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME)
#define HDCV_EC "/EC:128"
#else
#define HDCV_EC ""
#endif

/* RP — rotation period in seconds */
#if defined(CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC)
#define HDCV_RP "/RP:S" HUBBLE_STRINGIFY(CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC)
#else
#define HDCV_RP ""
#endif

/* N — network */
#if defined(CONFIG_HUBBLE_BLE_NETWORK) && defined(CONFIG_HUBBLE_SAT_NETWORK)
#define HDCV_N "/N:TS"
#elif defined(CONFIG_HUBBLE_BLE_NETWORK)
#define HDCV_N "/N:T"
#elif defined(CONFIG_HUBBLE_SAT_NETWORK)
#define HDCV_N "/N:S"
#else
#define HDCV_N ""
#endif

/* TV — terrestrial protocol version (present with terrestrial network) */
#if defined(CONFIG_HUBBLE_BLE_NETWORK)
#define HDCV_TV "/TV:0"
#else
#define HDCV_TV ""
#endif

/* SV — protocol version (present with satellite network) */
#if defined(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1)
#define HDCV_SV "/SV:0"
#else
#define HDCV_SV ""
#endif

#define HUBBLE_DEVICE_CONFIG_VECTOR                                            \
	"HDCV:1.0" HDCV_E HDCV_CS HDCV_EC HDCV_RP HDCV_N HDCV_TV HDCV_SV

#endif /* SRC_UTILS_HDCV_H */
