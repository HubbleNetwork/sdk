/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_VECTORS_H
#define TEST_VECTORS_H

#include <stddef.h>
#include <stdint.h>

#include <hubble/sat/packet.h>

/**
 * @brief Known-answer test vector for satellite packet construction.
 */
struct sat_packet_test_vector {
	const char *description; /* Human-readable test case name */
	uint32_t time_counter;   /* Time counter value */
	uint16_t seq_no;         /* Sequence number */
	const uint8_t *payload;  /* Cleartext payload input */
	size_t payload_len;      /* Payload length (0, 4, 9 or 13) */
	size_t expected_len;     /* Expected packet->length, in symbols */
	uint8_t expected[HUBBLE_PACKET_MAX_SIZE]; /* Expected packet->data */
};

/* Cleartext payload; the shorter cases use a prefix of this buffer. */
extern const uint8_t test_payload[13];

/* Master key the vectors were generated with. */
extern const uint8_t test_key[32];

extern const struct sat_packet_test_vector test_vectors[];
extern const size_t test_vectors_count;

#endif /* TEST_VECTORS_H */
