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

#define HUBBLE_SAT_CHANNEL_DEFAULT            5U

/* The interval (in seconds) packets are re-transmitted */
#define _SAT_RETRANSMISSION_INTERVAL_NORMAL_S 20U
#define _SAT_RETRANSMISSION_INTERVAL_HIGH_S   10U

/* The amount of times the same packet is transmitted */
#define _SAT_RETRANSMISSION_RETRIES_NORMAL    8U
#define _SAT_RETRANSMISSION_RETRIES_HIGH      16U


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
	uint8_t interval_s, retries, channel;

	ret = _transmission_params_get(mode, &retries, &interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING("Invalid mode given");
		return ret;
	}

	retries = HUBBLE_MIN(UINT8_MAX,
			     retries + _additional_retries_count(interval_s));

	ret = hubble_rand_get(&channel, sizeof(channel));
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Could not get a random channel, falling "
				   "back to default channel");
		channel = HUBBLE_SAT_CHANNEL_DEFAULT;
	} else {
		channel = channel % HUBBLE_SAT_NUM_CHANNELS;
	}

	ret = hubble_sat_port_packet_send(channel, packet, retries, interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}
