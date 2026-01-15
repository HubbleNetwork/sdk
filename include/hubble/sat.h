/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_SAT_H
#define INCLUDE_HUBBLE_SAT_H

#include <stdint.h>

#include <hubble/sat/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble Sat Network Function APIs
 * @defgroup hubble_sat_api Sattelite Network Function APIs
 * @{
 */

/**
 * @brief Transmit a packet using the Hubble satellite communication system.
 *
 * This function sends a packet over the satellite communication channel.
 * The packet must be properly formatted and adhere to the Hubble protocol.
 *
 * @param packet A pointer to the @ref hubble_sat_packet structure containing
 *               the data to be transmitted.
 *
 * @return 0 on successful transmission, or a negative error code on failure.
 *
 * @warning This function does not perform any validation on the packet
 *          structure. It is the caller's responsibility to ensure the
 *          packet is correctly formatted.
 */
int hubble_sat_packet_send(const struct hubble_sat_packet *packet);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_SAT_H */
