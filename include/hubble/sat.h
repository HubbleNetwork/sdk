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
 * @brief Initializes the Hubble satellite system.
 *
 * This function performs the necessary setup and initialization
 * for the Hubble satellite system. It ensures that all required
 * components are properly configured and ready for operation.
 *
 * @return int
 *   - 0 on successful initialization.
 *   - Negative value on failure, indicating the error code.
 *
 * @note This function must be called before any other Hubble satellite
 *       operations are performed.
 *
 * @warning Ensure that the system is in a safe state before calling
 *          this function to avoid unexpected behavior.
 */
int hubble_sat_init(void);

/**
 * @brief Enable the hardware needed for satellite transmissions.
 *
 * This function powers on the radio hardware and prepares it for communication.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int hubble_sat_enable(void);

/**
 * @brief Disable the hardware needed for satellite transmissions.
 *
 * This function powers off the radio hardware to save power when it
 * is not in use.
 */
void hubble_sat_disable(void);

/**
 * @brief Transmit a packet using the Hubble satellite communication system.
 *
 * This function sends a packet over the satellite communication channel.
 * The packet must be properly formatted and adhere to the Hubble protocol.
 *
 * @param packet A pointer to the ~hubble_packet~ structure containing the
 *               data to be transmitted.
 *
 * @return 0 on successful transmission, or a negative error code on failure.
 *
 * @note Ensure that the satellite hardware is enabled using ~hubble_sat_enable~
 *       before calling this function.
 *
 * @warning This function does not perform any validation on the packet
 *          structure. It is the caller's responsibility to ensure the
 *          packet is correctly formatted.
 */
int hubble_sat_transmit_packet(const struct hubble_sat_packet *packet);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_SAT_H */
