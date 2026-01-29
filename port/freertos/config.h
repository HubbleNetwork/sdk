/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_PORT_FREERTOS_CONFIG_H
#define INCLUDE_PORT_FREERTOS_CONFIG_H

/*
 * Enable the Hubble BLE and Satellite network modules.
 */
#define CONFIG_HUBBLE_BLE_NETWORK 1
#define CONFIG_HUBBLE_SAT_NETWORK 1

#if CONFIG_HUBBLE_BLE_NETWORK
/*
 * Size of the encryption key in bytes. Valid options are
 * 16 for 128 bits keys or 32 for 256 bits keys.
 */
#define CONFIG_HUBBLE_KEY_SIZE 16

/*
 * Frequency to change the counter timer.
 */
#define CONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY

#endif /* CONFIG_HUBBLE_BLE_NETWORK */

#if CONFIG_HUBBLE_SAT_NETWORK

/*
 * Use for fly by calculation. Enable this option
 * reduces code using polynomial approximation
 * for trigonometric functions
 */
/* #define CONFIG_HUBBLE_SAT_NETWORK_SMALL */

/*
 * Device time drift retry rate in parts per million (PPM).
 * Additional retries is added proportional to time since
 * last time the device had utc time synced.
 */
#define CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR  500

/* Protocol version
 *
 * Select only one of the following options:
 * - CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1: first version of sat
 * protocol. Channel hopping during transmissions.
 * - CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_DEPRECATED: deprecated version
 * of sat protocol. No channel hopping during transmissions.
 */
#define CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1 1
#if defined(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1) &&                          \
	defined(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_DEPRECATED)
#error "Only one protocol can be selected"
#endif

#endif /* CONFIG_HUBBLE_SAT_NETWORK */

#endif /* INCLUDE_PORT_FREERTOS_CONFIG_H */
