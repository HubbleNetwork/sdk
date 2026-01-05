/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
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

int hubble_sat_board_packet_send(uint8_t channel,
				 const struct hubble_sat_packet *packet)
{
	ARG_UNUSED(channel);
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
	zassert_ok(err);

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

static void *sat_test_setup(void)
{
	int err;

	err = hubble_init(_utc, sat_key);
	zassert_ok(err);

	return NULL;
}

ZTEST_SUITE(sat_test, NULL, sat_test_setup, NULL, NULL, NULL);
