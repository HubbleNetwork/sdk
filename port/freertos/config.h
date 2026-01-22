/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_PORT_FREERTOS_CONFIG_H
#define INCLUDE_PORT_FREERTOS_CONFIG_H

/*
 * Size of the encryption key in bytes. Valid options are
 * 16 for 128 bits keys or 32 for 256 bits keys.
 */
#define CONFIG_HUBBLE_KEY_SIZE                  16

/*
 * EID Generation Mode
 *
 * UTC-based mode is used by default. To use counter-based mode instead,
 * define CONFIG_HUBBLE_EID_COUNTER_BASED (and CONFIG_HUBBLE_EID_COUNTER_BITS).
 */
/* #define CONFIG_HUBBLE_EID_COUNTER_BASED  1 */

#if defined(CONFIG_HUBBLE_EID_UTC_BASED) &&                                    \
	defined(CONFIG_HUBBLE_EID_COUNTER_BASED)
#error "Cannot define both CONFIG_HUBBLE_EID_UTC_BASED and CONFIG_HUBBLE_EID_COUNTER_BASED"
#endif

/*
 * EID rotation period in seconds.
 * Range: 900-86400 (15 minutes to 24 hours)
 * Default: 86400 (24 hours / daily)
 */
#ifndef CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC
#define CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC 86400
#endif

/*
 * EID counter bit width (counter-based mode only).
 * Range: 4-11 bits (16 to 2048 unique EID epochs)
 *
 * Examples with default rotation period (86400 sec / daily):
 *   4 bits (16 values):   ~16 days of unique EIDs
 *   6 bits (64 values):   ~64 days of unique EIDs
 *   8 bits (256 values):  ~256 days of unique EIDs
 *   11 bits (2048 values): ~5.6 years of unique EIDs
 *
 * Must be defined when CONFIG_HUBBLE_EID_COUNTER_BASED is enabled.
 */
/* #define CONFIG_HUBBLE_EID_COUNTER_BITS 6 */

/*
 * DEPRECATED: Use CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC instead.
 * Kept for backward compatibility.
 */
#define CONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY

#endif /* INCLUDE_PORT_FREERTOS_CONFIG_H */
