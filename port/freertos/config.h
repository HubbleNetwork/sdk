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
 * EID rotation period in seconds.
 * Range: 900-86400 (15 minutes to 24 hours)
 * Default: 86400 (24 hours / daily)
 */
#ifndef CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC
#define CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC 86400
#endif

/*
 * DEPRECATED: Use CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC instead.
 * Kept for backward compatibility.
 */
#define CONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY

#endif /* INCLUDE_PORT_FREERTOS_CONFIG_H */
