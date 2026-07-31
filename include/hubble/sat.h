/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_SAT_H
#define INCLUDE_HUBBLE_SAT_H

#include <errno.h>
#include <stdint.h>

#include <hubble/sat/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble Sat Network Function APIs
 * @defgroup hubble_sat_api Satellite Network Function APIs
 * @{
 */

/**
 * @brief Satellite transmission mode
 *
 * It tells what is the desired reliability when transmitting
 * a packet. Higher reliability consumes higher power because it increases the
 * number of retries.
 *
 * @note The retry counts listed below are baselines. Extra retries are
 *       added to compensate for the device's clock drift accumulated since
 *       the last time synchronization: one additional retry is added for
 *       every full retransmission interval worth of drift. The longer the
 *       device goes without synchronizing its clock, the more retries are
 *       performed. Modes with no retries (@ref HUBBLE_SAT_RELIABILITY_NONE)
 *       are not affected.
 *
 *       The drift is estimated from the device's Time Drift Rate (TDR),
 *       configured through @c CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR and
 *       expressed in parts per million (PPM). The accumulated drift is
 *       computed as (time since last sync) * TDR, so a higher TDR yields
 *       more additional retries for the same elapsed time.
 */
enum hubble_sat_transmission_mode {
	/** No retries. The packet is transmitted one time. */
	HUBBLE_SAT_RELIABILITY_NONE,
	/**
	 * Good balance between reliability and power consumption.
	 * The packet is transmitted 8 times with a 20 second interval
	 * between transmissions.
	 */
	HUBBLE_SAT_RELIABILITY_NORMAL,
	/**
	 * High reliability and higher power consumption.
	 * The packet is transmitted 16 times with a 10 second interval
	 * between transmissions.
	 */
	HUBBLE_SAT_RELIABILITY_HIGH,
};

/**
 * @brief Transmit a packet using the Hubble satellite communication system.
 *
 * This function sends a packet over the satellite communication channel.
 * The packet must be properly formatted and adhere to the Hubble protocol.
 *
 * @note This function is blocking: it does not return until the transmission
 *       period has completed.
 *
 * @param packet A pointer to the @ref hubble_sat_packet structure containing
 *               the data to be transmitted.
 * @param mode   Desired reliability for the transmission.
 *
 * @return 0 on successful transmission, or a negative error code on failure.
 *
 * @warning This function checks if the packet is NULL but does not perform
 *          any validation on the packet structure. It is the caller's
 *          responsibility to ensure the packet is correctly formatted.
 */
int hubble_sat_packet_send(const struct hubble_sat_packet *packet,
			   enum hubble_sat_transmission_mode mode);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_SAT_H */
