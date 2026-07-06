/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/port/sat_radio.h>
#include <hubble/sat.h>
#include <hubble/sat/packet.h>

#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include <stdint.h>
#include <stdlib.h>

#define HUBBLE_SAT_DEV_ID 0x1337

static uint64_t _unix_time = 1760210751803ULL;
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
static bool _test_hopping = false;
static int8_t _hopping_info = -1;
static uint8_t _last_channel = 0;

static int _hopping_sequence_find(uint8_t channel1, uint8_t channel2,
				  uint8_t channel3)
{
	for (uint8_t i = 0; i < ARRAY_SIZE(_channel_hops); i++) {
		for (uint8_t j = 0; j < HUBBLE_SAT_NUM_CHANNELS; j++) {
			if (channel1 == _channel_hops[i][j]) {
				if ((channel2 ==
				     _channel_hops[i][(j + 1) %
						      HUBBLE_SAT_NUM_CHANNELS]) &&
				    (channel3 ==
				     _channel_hops[i][(j + 2) %
						      HUBBLE_SAT_NUM_CHANNELS])) {
					return i;
				}
			}
		}
	}

	return -1;
}

static int _channel_hopping_check(uint8_t channel1, uint8_t channel2,
				  uint8_t channel3)
{
	uint8_t expected_channel1 = 0;
	uint8_t expected_channel2 = 0;
	uint8_t expected_channel3 = 0;

	for (uint8_t i = 0; i < HUBBLE_SAT_NUM_CHANNELS; i++) {
		if (_channel_hops[_hopping_info][i] == _last_channel) {
			expected_channel1 =
				_channel_hops[_hopping_info]
					     [(i + 1) % HUBBLE_SAT_NUM_CHANNELS];
			expected_channel2 =
				_channel_hops[_hopping_info]
					     [(i + 2) % HUBBLE_SAT_NUM_CHANNELS];
			expected_channel3 =
				_channel_hops[_hopping_info]
					     [(i + 3) % HUBBLE_SAT_NUM_CHANNELS];
			break;
		}
	}

	return (expected_channel1 == channel1) &&
	       (expected_channel2 == channel2) &&
	       (expected_channel3 == channel3);
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	int ret = 0;

	if (_test_hopping) {
		if (_hopping_info == -1) {
			ret = _hopping_sequence_find(packet->frame[0].channel,
						     packet->frame[1].channel,
						     packet->frame[2].channel);
			if (ret >= 0) {
				_hopping_info = ret;
			}
		} else {
			ret = _channel_hopping_check(packet->frame[0].channel,
						     packet->frame[1].channel,
						     packet->frame[2].channel);
		}
		_last_channel = packet->frame[2].channel;
	}

	_transmission_count--;

	return ret;
}

ZTEST(sat_test, test_packet)
{
	int err;
	uint8_t buffer[64] = {0};
	struct hubble_sat_packet pkt;

	/* Packet without data is valid */
	err = hubble_sat_packet_get(&pkt, NULL, 0);
	zassert_ok(err);

	/* NULL packet is invalid */
	err = hubble_sat_packet_get(NULL, NULL, 0);
	zassert_not_ok(err);

	/* NULL payload with invalid length must return error */
	err = hubble_sat_packet_get(&pkt, NULL, 20);
	zassert_not_ok(err);

	err = hubble_sat_packet_get(&pkt, buffer, 1);

	/* Available sizes are: 0, 4, 9 and 13 (HUBBLE_SAT_PAYLOAD_MAX) */
	zassert_not_ok(err);

	/* Additional check to confirm it. */
	err = hubble_sat_packet_get(&pkt, buffer, 4);
	zassert_ok(err);

	err = hubble_sat_packet_get(&pkt, buffer, 9);
	zassert_ok(err);

	err = hubble_sat_packet_get(&pkt, buffer, HUBBLE_SAT_PAYLOAD_MAX);
	zassert_ok(err);

	/* Let's try beyond the max size. It must fail */
	err = hubble_sat_packet_get(&pkt, buffer, HUBBLE_SAT_PAYLOAD_MAX + 1);
	zassert_not_ok(err);
}

ZTEST(sat_test, test_profile)
{
	int err;
	struct hubble_sat_packet pkt;

	/* Packet without data is valid */
	err = hubble_sat_packet_get(&pkt, NULL, 0);
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

	/* Test no reliability. One time transmission */
	_transmission_count = 1U;
	err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);

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
	int err;
	struct hubble_sat_packet pkt;

	_test_hopping = true;
	_transmission_count = 2;

	/* Packet without data is valid */
	err = hubble_sat_packet_get(&pkt, NULL, 0);
	zassert_ok(err);

	err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE);
	zassert_ok(err);

	err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE);
	zassert_ok(err);

	_test_hopping = false;
}

ZTEST(sat_test, test_packet_send_from_pass)
{
	int err;
	struct hubble_sat_packet pkt;
	struct hubble_sat_pass_info pass = {0};

	err = hubble_sat_packet_get(&pkt, NULL, 0);
	zassert_ok(err);

	/* NULL packet must fail */
	err = hubble_sat_packet_pass_send(NULL, HUBBLE_SAT_RELIABILITY_NORMAL,
					  &pass);
	zassert_not_ok(err);

	/* NULL pass must fail */
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL,
					  NULL);
	zassert_not_ok(err);

	/* Invalid mode must fail without transmitting */
	_transmission_count = 8U;
	err = hubble_sat_packet_pass_send(&pkt, 255, &pass);
	zassert_not_ok(err);
	zassert_equal(8U, _transmission_count);

	/* RELIABILITY_NONE is invalid: a single shot does not cover a pass */
	pass.duration = 160000U;
	_transmission_count = 1U;
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE,
					  &pass);
	zassert_not_ok(err);
	zassert_equal(1U, _transmission_count);

	/* RELIABILITY_NORMAL (20s interval): 160s pass -> 8 retries */
	pass.duration = 160000U;
	_transmission_count = 8U;
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL,
					  &pass);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);

	/* RELIABILITY_HIGH (10s interval): 160s pass -> 16 retries */
	pass.duration = 160000U;
	_transmission_count = 16U;
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_HIGH,
					  &pass);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);

	/* Shorter pass: 40s with NORMAL -> 2 retries, not the fixed 8 */
	pass.duration = 40000U;
	_transmission_count = 2U;
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL,
					  &pass);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);

	/* Pass shorter than one interval -> minimum 1 retry */
	pass.duration = 5000U;
	_transmission_count = 1U;
	err = hubble_sat_packet_pass_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL,
					  &pass);
	zassert_ok(err);
	zassert_equal(0, _transmission_count);
}

static void *sat_test_setup(void)
{
	int err;

	err = hubble_init(_unix_time, sat_key);
	zassert_ok(err);

	return NULL;
}

ZTEST_SUITE(sat_test, NULL, sat_test_setup, NULL, NULL, NULL);
