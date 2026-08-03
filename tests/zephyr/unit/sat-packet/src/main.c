/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for hubble_sat_packet_get().
 *
 */

#include <hubble/hubble.h>
#include <hubble/sat.h>
#include <hubble/sat/packet.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <string.h>

#include "test_vectors.h"

#define ROTATION_PERIOD_MS 86400000ULL

/* Sequence counter override (CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM). */
static uint16_t test_seq_override;

uint16_t hubble_sequence_counter_get(void)
{
	return test_seq_override;
}

/* Pin uptime to 0 so hubble_time_get() returns exactly what we passed in. */
uint64_t hubble_uptime_get(void)
{
	return 0;
}

int hubble_sat_board_init(void)
{
	return 0;
}

int hubble_sat_board_enable(void)
{
	return 0;
}

int hubble_sat_board_disable(void)
{
	return 0;
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	ARG_UNUSED(packet);

	return 0;
}

/* Payload length -> (payload symbols, ECC symbols), per the protocol tables in
 * src/hubble_sat_packet.c. packet->length must be their sum.
 */
static const struct {
	size_t payload_len;
	size_t symbols;
	size_t ecc;
} length_table[] = {
	{0, 13, 10},
	{4, 18, 12},
	{9, 25, 14},
	{13, 30, 16},
};

static int packet_get(struct hubble_sat_packet *packet, uint32_t time_counter,
		      uint16_t seq_no, const uint8_t *payload, size_t len)
{
	int ret = hubble_init((uint64_t)time_counter * ROTATION_PERIOD_MS,
			      test_key);

	zassert_ok(ret, "hubble_init failed: %d", ret);
	test_seq_override = seq_no;

	memset(packet, 0, sizeof(*packet));

	return hubble_sat_packet_get(packet, payload, len);
}

/*
 * Primary regression test: the full packet must match byte-for-byte.
 */
ZTEST(sat_packet_test, test_packet_known_answer)
{
	for (size_t i = 0; i < test_vectors_count; i++) {
		const struct sat_packet_test_vector *tv = &test_vectors[i];
		struct hubble_sat_packet packet;

		int ret = packet_get(&packet, tv->time_counter, tv->seq_no,
				     tv->payload, tv->payload_len);

		zassert_ok(ret, "Vector %zu (%s) failed with error %d", i,
			   tv->description, ret);

		zassert_equal(packet.length, tv->expected_len,
			      "Vector %zu (%s) length mismatch: got %zu, "
			      "expected %zu",
			      i, tv->description, packet.length,
			      tv->expected_len);

		zassert_mem_equal(packet.data, tv->expected, tv->expected_len,
				  "Vector %zu (%s) packet mismatch", i,
				  tv->description);
	}
}

/*
 * The parity block must land at the end of the systematic data, not at
 * symbols[ecc]. When it does not, the symbols past 2*ecc are never written and
 * stay zero for every input — so they cannot vary with the sequence number.
 *
 * This catches the same defect as the known-answer test without depending on
 * the golden data, so it keeps holding if the vectors are ever regenerated.
 */
ZTEST(sat_packet_test, test_packet_tail_is_populated)
{
	for (size_t i = 0; i < ARRAY_SIZE(length_table); i++) {
		size_t len = length_table[i].payload_len;
		size_t tail = 2U * length_table[i].ecc;
		struct hubble_sat_packet first, second;
		int ret;

		zassert_true(
			tail < length_table[i].symbols + length_table[i].ecc,
			"test bug: no tail region for payload_len=%zu", len);

		ret = packet_get(&first, 20000U, 1U, test_payload, len);
		zassert_ok(ret, "payload_len=%zu failed: %d", len, ret);

		ret = packet_get(&second, 20000U, 2U, test_payload, len);
		zassert_ok(ret, "payload_len=%zu failed: %d", len, ret);

		zassert_true(
			memcmp(&first.data[tail], &second.data[tail],
			       first.length - tail) != 0,
			"payload_len=%zu: symbols [%zu..%zu) are identical "
			"for two different sequence numbers - the parity "
			"block was written at the wrong offset and the "
			"packet tail was never populated",
			len, tail, first.length);
	}
}

/*
 * packet->length must be payload symbols + ECC symbols for every supported
 * payload size. A parity block written at the wrong offset does not change
 * this, so it is a companion check rather than a regression test on its own.
 */
ZTEST(sat_packet_test, test_packet_length_table)
{
	for (size_t i = 0; i < ARRAY_SIZE(length_table); i++) {
		struct hubble_sat_packet packet;
		size_t expected = length_table[i].symbols + length_table[i].ecc;

		int ret = packet_get(&packet, 20000U, 0U, test_payload,
				     length_table[i].payload_len);

		zassert_ok(ret, "payload_len=%zu failed: %d",
			   length_table[i].payload_len, ret);
		zassert_equal(packet.length, expected,
			      "payload_len=%zu: got length %zu, expected %zu",
			      length_table[i].payload_len, packet.length,
			      expected);
		zassert_true(packet.length <= HUBBLE_PACKET_MAX_SIZE,
			     "payload_len=%zu: length %zu exceeds "
			     "HUBBLE_PACKET_MAX_SIZE",
			     length_table[i].payload_len, packet.length);
	}
}

/*
 * Only 0, 4, 9 and 13 byte payloads are supported. Both of the switches that
 * map a payload length (size and ECC) must reject everything else; letting an
 * unsupported length through is what f329e45 originally set out to fix.
 */
ZTEST(sat_packet_test, test_packet_invalid_length)
{
	for (size_t len = 1; len <= 14; len++) {
		struct hubble_sat_packet packet;
		int ret;

		if ((len == 4U) || (len == 9U) || (len == 13U)) {
			continue;
		}

		ret = packet_get(&packet, 20000U, 0U, test_payload, len);
		zassert_equal(ret, -EINVAL,
			      "payload_len=%zu should be rejected, got %d", len,
			      ret);
	}
}

ZTEST(sat_packet_test, test_packet_null_args)
{
	struct hubble_sat_packet packet;
	int ret = hubble_init(0xdeadbeef, test_key);

	zassert_ok(ret, "hubble_init failed: %d", ret);
	test_seq_override = 0;

	zassert_equal(hubble_sat_packet_get(NULL, test_payload, 4), -EINVAL,
		      "NULL packet should return -EINVAL");
	zassert_equal(
		hubble_sat_packet_get(&packet, NULL, 4), -EINVAL,
		"NULL payload with non-zero length should return -EINVAL");
	zassert_ok(hubble_sat_packet_get(&packet, NULL, 0),
		   "NULL payload with length 0 should succeed");
}

static void *sat_packet_test_setup(void)
{
	test_seq_override = 0;

	return NULL;
}

ZTEST_SUITE(sat_packet_test, NULL, sat_packet_test_setup, NULL, NULL, NULL);
