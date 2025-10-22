/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 **/

/**
 * @file packet.h
 * @brief Hubble Network packet APIs
 **/

#ifndef INCLUDE_HUBBLE_SAT_PACKET_H
#define INCLUDE_HUBBLE_SAT_PACKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max number of symbols that packet can have */
#define HUBBLE_PACKET_MAX_SIZE 44

/**
 * @brief Structure representing a Hubble packet.
 *
 * This structure is used to represent a Hubble packet, the data
 * is a set symbols that represents the number of frequency steps
 * that should be added from the reference frequency (channel).
 *
 * Since the preamble is a fixed pattern that is formed by the reference
 * frequency and the complete absence of tranmission for a certain period,
 * it is NOT included in this structure.
 *
 * @note The preamble is NOT included in this struct.
 */
struct hubble_sat_packet {
	/**
	 * @brief Data of the packet.
	 */
	uint8_t data[HUBBLE_PACKET_MAX_SIZE];
	/**
	 * @brief Number of symbols in the packet.
	 */
	size_t length;
};

int hubble_sat_packet_get(struct hubble_sat_packet *packet, uint64_t dev_id,
			  const void *payload, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_SAT_PACKET_H */
