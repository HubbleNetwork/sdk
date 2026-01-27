/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>

#include <hubble/sat.h>
#include <hubble/port/sys.h>
#include <hubble/port/sat_radio.h>

#include "hubble_priv.h"
#include "utils/macros.h"

/* Two bits used -> 2**2 */
#define _SAT_HOPPING_SEQUENCE_INFO_NUM        4U

/* The interval (in seconds) packets are re-transmitted */
#define _SAT_RETRANSMISSION_INTERVAL_NORMAL_S 20U
#define _SAT_RETRANSMISSION_INTERVAL_HIGH_S   10U

/* The amount of times the same packet is transmitted */
#define _SAT_RETRANSMISSION_RETRIES_NORMAL    8U
#define _SAT_RETRANSMISSION_RETRIES_HIGH      16U

/* This is pseudorandom pre-computed list of channel hopping. */
static uint8_t _channel_hops[_SAT_HOPPING_SEQUENCE_INFO_NUM][HUBBLE_SAT_NUM_CHANNELS] = {
	{3, 14, 5, 6, 9, 2, 12, 8, 15, 4, 11, 13, 17, 10, 1, 7, 0, 18, 16},
	{10, 3, 15, 5, 0, 17, 13, 6, 11, 4, 8, 18, 9, 14, 1, 12, 7, 16, 2},
	{14, 5, 11, 3, 8, 2, 18, 4, 10, 13, 9, 1, 16, 17, 0, 6, 15, 12, 7},
	{7, 0, 11, 18, 4, 2, 13, 5, 10, 17, 3, 9, 16, 14, 8, 12, 1, 6, 15},
};

static uint8_t _channel_idx_find(uint8_t hopping_sequence, uint8_t initial_channel)
{
	for (uint8_t idx = 0; idx < HUBBLE_SAT_NUM_CHANNELS; idx++) {
		if (_channel_hops[hopping_sequence][idx] == initial_channel) {
			return idx;
		}
	}

	/* This condition should never happen */
	return 0;
}

int hubble_sat_channel_next_hop_get(uint8_t hopping_sequence, uint8_t channel,
				    uint8_t *next_channel)
{
	uint8_t idx;

	if ((hopping_sequence >= _SAT_HOPPING_SEQUENCE_INFO_NUM) ||
	    (channel >= HUBBLE_SAT_NUM_CHANNELS) || (next_channel == NULL)) {
		return -EINVAL;
	}

	idx = (_channel_idx_find(hopping_sequence, channel) + 1) %
	      HUBBLE_SAT_NUM_CHANNELS;
	*next_channel = _channel_hops[hopping_sequence][idx];

	return 0;
}

static int _transmission_params_get(enum hubble_sat_transmission_mode mode,
				    uint8_t *retries, uint8_t *interval_s)
{
	int ret = 0;

	switch (mode) {
	case HUBBLE_SAT_RELIABILITY_NORMAL:
		*interval_s = _SAT_RETRANSMISSION_INTERVAL_NORMAL_S;
		*retries = _SAT_RETRANSMISSION_RETRIES_NORMAL;
		break;
	case HUBBLE_SAT_RELIABILITY_HIGH:
		*interval_s = _SAT_RETRANSMISSION_INTERVAL_HIGH_S;
		*retries = _SAT_RETRANSMISSION_RETRIES_HIGH;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static uint8_t _additional_retries_count(uint8_t interval_s)
{
	uint64_t synced_interval_s =
		(hubble_internal_utc_time_get() -
		 hubble_internal_utc_time_last_synced_get()) /
		1000;

	return HUBBLE_MIN(UINT8_MAX, (synced_interval_s *
				      CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR) /
					     (1000000ULL * interval_s));
}

int hubble_sat_packet_send(const struct hubble_sat_packet *packet,
			   enum hubble_sat_transmission_mode mode)
{
	int ret;
	uint8_t interval_s, retries;

	if (packet == NULL) {
		return -EINVAL;
	}

	ret = _transmission_params_get(mode, &retries, &interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING("Invalid mode given");
		return ret;
	}

	retries = HUBBLE_MIN(UINT8_MAX,
			     retries + _additional_retries_count(interval_s));

	ret = hubble_sat_port_packet_send(packet, retries, interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}
