/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>

#include <hubble/hubble.h>
#include <hubble/sat.h>
#include <hubble/port/sys.h>
#include <hubble/port/sat_radio.h>

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE
#include <hubble/sat/dtm.h>
#endif

#include "hubble_priv.h"
#include "utils/macros.h"

/* The interval (in seconds) packets are re-transmitted */
#define _SAT_RETRANSMISSION_INTERVAL_NORMAL_S 20U
#define _SAT_RETRANSMISSION_INTERVAL_HIGH_S   10U

/* The amount of times the same packet is transmitted */
#define _SAT_RETRANSMISSION_RETRIES_NORMAL    8U
#define _SAT_RETRANSMISSION_RETRIES_HIGH      16U

/* The expected drift should not exceed 90 min */
#define _SAT_MAX_TIME_DRIFT_MSEC              (90U * 60U * 1000U)

static int _transmission_params_get(enum hubble_sat_transmission_mode mode,
				    uint8_t *retries, uint8_t *interval_s)
{
	int ret = 0;

	switch (mode) {
	case HUBBLE_SAT_RELIABILITY_NONE:
		*interval_s = 0U;
		*retries = 1U;
		break;
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

uint32_t hubble_internal_time_drift_get(void)
{
	uint64_t elapsed_ms;
	uint64_t drift_ms;

	elapsed_ms = hubble_time_get() - hubble_internal_time_last_synced_get();

	drift_ms =
		(elapsed_ms * CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR) / 1000000ULL;

	return (uint32_t)HUBBLE_MIN(_SAT_MAX_TIME_DRIFT_MSEC, drift_ms);
}

static uint8_t _additional_retries_count(uint8_t interval_s)
{
	uint8_t ret;
	uint32_t drift_ms;

	if (interval_s == 0U) {
		return 0;
	}

	drift_ms = hubble_internal_time_drift_get();

	HUBBLE_LOG_DEBUG("Time drift since last sync: %u ms", drift_ms);

	ret = HUBBLE_MIN(UINT8_MAX, drift_ms / (1000U * interval_s));

	HUBBLE_LOG_DEBUG("Number of additional retries due TDR: %u", ret);

	return ret;
}

uint32_t hubble_internal_sat_transmission_period_get(void)
{
	uint8_t additional_retries =
		_additional_retries_count(_SAT_RETRANSMISSION_INTERVAL_NORMAL_S);

	/* x1000U to return the interval in ms */
	return ((_SAT_RETRANSMISSION_RETRIES_NORMAL + additional_retries) *
		_SAT_RETRANSMISSION_INTERVAL_NORMAL_S) *
	       1000U;
}

int hubble_internal_sat_init(void)
{
	int ret;

	ret = hubble_sat_port_init();
	if (ret != 0) {
		HUBBLE_LOG_ERROR(
			"Hubble Satellite Network initialization failed");
		return ret;
	}

	hubble_internal_channel_hopping_sequence_set();

	return 0;
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

	HUBBLE_LOG_DEBUG("Number of retries: %u - interval: %u seconds",
			 retries, interval_s);

	ret = hubble_sat_port_packet_send(packet, retries, interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}

int hubble_sat_packet_pass_send(const struct hubble_sat_packet *packet,
				enum hubble_sat_transmission_mode mode,
				const struct hubble_sat_pass_info *pass)
{
	int ret;
	uint8_t interval_s, retries;

	if (packet == NULL || pass == NULL) {
		return -EINVAL;
	}

	ret = _transmission_params_get(mode, &retries, &interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING("Invalid mode given");
		return ret;
	}

	if (interval_s > 0U) {
		retries = (uint8_t)HUBBLE_MIN(
			UINT8_MAX,
			HUBBLE_MAX(1U, pass->duration / (1000U * interval_s)));
	}

	HUBBLE_LOG_DEBUG("Number of retries: %u - interval: %u seconds",
			 retries, interval_s);

	ret = hubble_sat_port_packet_send(packet, retries, interval_s);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE

int hubble_sat_dtm_packet_send(enum hubble_sat_dtm_packet_type type,
			       int8_t channel)
{
	struct hubble_sat_packet packet;
	/* Hold max possible user data */
	char buffer[13];
	uint8_t len;
	int ret;

	if (type == HUBBLE_SAT_DTM_PACKET_SINGLE_FRAME) {
		packet.length = HUBBLE_SAT_DTM_PACKET_ONE_FRAME_ONLY_LEN;
		goto end;
	}
	switch (type) {
	case HUBBLE_SAT_DTM_PACKET_0:
		len = 0;
		break;
	case HUBBLE_SAT_DTM_PACKET_4:
		len = 4;
		break;
	case HUBBLE_SAT_DTM_PACKET_9:
		len = 9;
		break;
	case HUBBLE_SAT_DTM_PACKET_13:
		len = 13;
		break;
	default:
		return -EINVAL;
	}

	hubble_rand_get((uint8_t *)buffer, len);

	ret = hubble_sat_packet_get(&packet, buffer, len);
	if (ret < 0) {
		HUBBLE_LOG_WARNING("Hubble Satellite dtm packet get failed");
		return ret;
	}

end:
	ret = hubble_sat_dtm_port_packet_send(&packet, channel);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite dtm packet transmission failed");
		return ret;
	}

	return 0;
}

int hubble_sat_dtm_power_set(int8_t power)
{
	return hubble_sat_dtm_port_power_set(power);
}

int hubble_sat_dtm_cw_start(uint8_t channel)
{
	return hubble_sat_dtm_port_cw_start(channel);
}

int hubble_sat_dtm_cw_stop(void)
{
	return hubble_sat_dtm_port_cw_stop();
}

#endif
