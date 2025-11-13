/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
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
 * @brief Hubble Satellite Network porting API.
 * @defgroup hubble_sat_port Hubble Satellite Network porting APIs
 *
 * Functions and data structures that are used to port the Hubble Satellite
 * Network for different platforms.
 *
 * @{
 */
struct hubble_sat_api {
	/**
	 * @brief Transmits a packet.
	 *
	 * @param packet A pointer to a hubble_packet structure containing the
	 *               data to be transmitted.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*transmit_packet)(const struct hubble_sat_packet *packet);

	/**
	 * @brief Enables Hubble Network satellite stack.
	 *
	 * Setup everything that is need to transmit data.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*enable)(void);

	/**
	 * @brief Disables Hubble Network satellite stack.
	 *
	 * This API disables the satellite stack and restore devices to the
	 * state they were before.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*disable)(void);
};

/**
 * @brief Retrieves the satellite API interface.
 *
 * This function provides access to the satellite API, which includes logging
 * and packet transmission functionalities. The API is typically implemented
 * as a singleton.
 *
 * @return A pointer to a constant struct hubble_sat_api.
 *         Returns NULL if the API is unavailable.
 */
const struct hubble_sat_api *hubble_sat_api_get(void);

/** @} */ /* hubble_sat_port */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_PORT_SAT_RADIO_H */
