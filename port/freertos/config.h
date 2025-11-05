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
 * Frequency to change the counter timer.
 */
#define CONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY

#endif /* INCLUDE_PORT_FREERTOS_CONFIG_H */
