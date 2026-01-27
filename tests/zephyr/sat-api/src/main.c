/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/port/sat_radio.h>
#include <hubble/sat.h>
#include <hubble/sat/packet.h>

#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include <stdint.h>
#include <stdlib.h>

#define HUBBLE_SAT_DEV_ID 0x1337

static uint64_t _utc = 1760210751803ULL;
/* zRWlq8BgtnKIph5E6ZW6d9FAvUZWS4jeQcFaknOwzoU= */
static uint8_t sat_key[CONFIG_HUBBLE_KEY_SIZE] = {
	0xcd, 0x15, 0xa5, 0xab, 0xc0, 0x60, 0xb6, 0x72, 0x88, 0xa6, 0x1e,
	0x44, 0xe9, 0x95, 0xba, 0x77, 0xd1, 0x40, 0xbd, 0x46, 0x56, 0x4b,
	0x88, 0xde, 0x41, 0xc1, 0x5a, 0x92, 0x73, 0xb0, 0xce, 0x85};

static uint8_t _channel_hops[][HUBBLE_SAT_NUM_CHANNELS] = {
	{3, 14, 5, 6, 9, 2, 12, 8, 15, 4, 11, 13, 17, 10, 1, 7, 0, 18, 16},
	{10, 3, 15, 5, 0, 17, 13, 6, 11, 4, 8, 18, 9, 14, 1, 12, 7, 16, 2},
	{14, 5, 11, 3, 8, 2, 18, 4, 10, 13, 9, 1, 16, 17, 0, 6, 15, 12, 7},
	{7, 0, 11, 18, 4, 2, 13, 5, 10, 17, 3, 9, 16, 14, 8, 12, 1, 6, 15},
};

/* Implement sat board support. */
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

static uint8_t _transmission_count;

int hubble_sat_board_packet_send(const struct hubble_sat_packet *packet)
{
	ARG_UNUSED(packet);

	_transmission_count--;

	return 0;
}

ZTEST(sat_test, test_packet)
{
	int err;
	uint8_t buffer[64] = {0};
	struct hubble_sat_packet pkt;

	/* Packet without data is valid */
	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, NULL, 0);
	zassert_ok(err);

	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, buffer, 1);
#ifdef CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_DEPRECATED
	zassert_ok(err);
#else
	/* Available sizes are: 0, 4, 9 and 13 (HUBBLE_SAT_PAYLOAD_MAX) */
	zassert_not_ok(err);

	/* Additional check to confirm it. */
	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, buffer, 4);
	zassert_ok(err);

	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, buffer, 9);
	zassert_ok(err);
#endif

	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, buffer,
				    HUBBLE_SAT_PAYLOAD_MAX);
	zassert_ok(err);

	/* Let's try beyond the max size. It must fail */
	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, buffer,
				    HUBBLE_SAT_PAYLOAD_MAX + 1);
	zassert_not_ok(err);
}

ZTEST(sat_test, test_profile)
{
	int err;
	struct hubble_sat_packet pkt;

	/* Packet without data is valid */
	err = hubble_sat_packet_get(&pkt, HUBBLE_SAT_DEV_ID, NULL, 0);
	zassert_ok(err);

	/* Sanity check. Invalid packet */
	err = hubble_sat_packet_send(NULL, HUBBLE_SAT_RELIABILITY_NORMAL);
	zassert_not_ok(err);

	/* Sanity check. Invalid reliability */
	_transmission_count = 16U;
	err = hubble_sat_packet_send(&pkt, 255);
	zassert_not_ok(err);
	/* Checking no transmissions happened. */
	zassert_equal(16U, _transmission_count);

	/* Test normal reliability  - 8u */
	_transmission_count = 8U;
	err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);

	/* Test high reliability  - 16u */
	_transmission_count = 16U;
	err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_HIGH);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);
}

ZTEST(sat_test, test_channel_hopping)
{
	int ret;
	uint8_t channel_hop;

	/* Some sanity API check */
	ret = hubble_sat_channel_next_hop_get(0, 0, NULL);
	zassert_not_ok(ret);

	ret = hubble_sat_channel_next_hop_get(5, 0, &channel_hop);
	zassert_not_ok(ret);

	ret = hubble_sat_channel_next_hop_get(0, HUBBLE_SAT_NUM_CHANNELS,
					      &channel_hop);
	zassert_not_ok(ret);

	for (uint8_t sequence = 0; sequence < ARRAY_SIZE(_channel_hops);
	     sequence++) {
		for (uint8_t i = 0; i < HUBBLE_SAT_NUM_CHANNELS; i++) {
			uint8_t channel = _channel_hops[sequence][i];
			uint8_t channel_expected =
				_channel_hops[sequence]
					     [(i + 1) % HUBBLE_SAT_NUM_CHANNELS];

			ret = hubble_sat_channel_next_hop_get(sequence, channel,
							      &channel_hop);
			zassert_ok(ret);
			zassert_equal(channel_expected, channel_hop);
		}
	}
}

static void *sat_test_setup(void)
{
	int err;

	err = hubble_init(_utc, sat_key);
	zassert_ok(err);

	return NULL;
}

ZTEST_SUITE(sat_test, NULL, sat_test_setup, NULL, NULL, NULL);
