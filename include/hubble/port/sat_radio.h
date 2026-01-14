/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sat_radio.h
 * @brief Hubble Network satellite radio port APIs
 */

#ifndef INCLUDE_HUBBLE_PORT_SAT_RADIO_H
#define INCLUDE_HUBBLE_PORT_SAT_RADIO_H

#include <stddef.h>
#include <stdint.h>

#include <hubble/sat/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble Network Satellite Radio Port APIs
 *
 * Platform-specific functions that need to be implemented for
 * satellite communication.
 *
 * @defgroup hubble_sat_radio_port Hubble Network Satellite Radio Port APIs
 *
 * @{
 */

/**
 * @brief Duration to wait for a symbol transmission in microseconds.
 */
#define HUBBLE_WAIT_SYMBOL_US        8000U

/**
 * @brief Duration to wait for a symbol off period in microseconds.
 */
#define HUBBLE_WAIT_SYMBOL_OFF_US    1600U

/**
 * @brief Duration to wait for preamble off in microseconds.
 */
#define HUBBLE_WAIT_PREAMBLE_US      9600U

/**
 * @brief Number of available channels for transmissions.
 */
#define HUBBLE_SAT_NUM_CHANNELS      19U

/**
 * @brief Preamble sequence pattern for satellite communication.
 *
 * This array defines the frequency step pattern used for the preamble.
 * Values represent frequency steps relative to the reference frequency:
 * -  0: reference frequency
 * - -1: no transmission
 */
#define HUBBLE_SAT_PREAMBLE_SEQUENCE (int8_t[]){0, -1, 0, -1, 0, -1, 0, 0}

/**
 * @brief Initialize the satellite radio port.
 *
 * This function performs platform-specific initialization of the satellite
 * radio hardware. It is called before any other satellite radio
 * operations are performed.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_sat_port_init(void);

/**
 * @brief Transmit a packet over the satellite radio.
 *
 * This function transmits a packet using the satellite radio hardware.
 * The packet is sent on the specified channel (frequency) using the
 * platform-specific radio implementation. It handles re-transmissions
 * internally.
 *
 * @note This function blocks the caller during the whole transmission.
 * @note This API is thread safe.
 *
 * @param channel The channel (frequency) to transmit on.
 * @param packet Pointer to the packet structure containing the data to transmit.
 * @param retries The number of times this packet must be transmit.
 * @param interval_s The time interval between transmissions.
 *
 * @return 0 on successful transmission, negative error code on failure.
 */
int hubble_sat_port_packet_send(uint8_t channel,
				const struct hubble_sat_packet *packet,
				uint8_t retries, uint8_t interval_s);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_PORT_SAT_RADIO_H */
