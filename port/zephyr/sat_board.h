/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sat_board.h
 * @internal
 * @brief Hubble Network Zephyr board-specific satellite communication APIs
 */

#ifndef PORT_ZEPHYR_SAT_BOARD_H
#define PORT_ZEPHYR_SAT_BOARD_H

#include <hubble/sat/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the board hardware for satellite communication.
 *
 * This function performs the necessary setup and initialization for the
 * board-specific hardware components required for satellite communication,
 * such as front-end module (FEM) configuration and radio power settings.
 *
 * @return 0 on successful initialization, or a negative error code on failure.
 *
 * @note This function must be called before any other board-specific
 *       satellite operations are performed.
 */
int hubble_sat_board_init(void);

/**
 * @brief Enables the board hardware for satellite transmission.
 *
 * This function enables the board hardware components necessary for satellite
 * packet transmission, including setting the radio transmit power and enabling
 * the power amplifier (PA).
 *
 * @return 0 on success, or a negative error code on failure.
 *
 * @note This function should be called before transmitting packets.
 *       Call hubble_sat_board_disable() after transmission is complete.
 */
int hubble_sat_board_enable(void);

/**
 * @brief Disables the board hardware after satellite transmission.
 *
 * This function disables the board hardware components used for satellite
 * transmission, typically called after packet transmission is complete to
 * conserve power.
 *
 * @return 0 on success, or a negative error code on failure.
 *
 * @note This function should be called after transmission operations
 *       are complete to properly shut down the hardware.
 */
int hubble_sat_board_disable(void);

/**
 * @brief Transmits a packet using the board hardware.
 *
 * This function sends a packet over the satellite communication channel using
 * the board-specific hardware. The packet must be properly formatted according
 * to the Hubble satellite protocol.
 *
 * @param channel Channel used to transmit the given packet.
 * @param packet A pointer to the hubble_sat_packet structure containing the
 *               data to be transmitted.
 *
 * @return 0 on successful transmission, or a negative error code on failure.
 *
 * @note The board hardware must be enabled using hubble_sat_board_enable()
 *       before calling this function.
 */
int hubble_sat_board_packet_send(uint8_t channel,
				 const struct hubble_sat_packet *packet);

#ifdef __cplusplus
}
#endif

#endif /* PORT_ZEPHYR_SAT_BOARD_H */
