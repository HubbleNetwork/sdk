/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>

#include "reed_solomon_encoder.h"

#include <hubble/sat.h>
#include <hubble/hubble_port.h>

#define HUBBLE_LOG(_level, ...)                                                \
	do {                                                                   \
		hubble_log((_level), __VA_ARGS__);                             \
	} while (0)

#define HUBBLE_LOG_DEBUG(...) HUBBLE_LOG(HUBBLE_LOG_DEBUG, __VA_ARGS__)
#define HUBBLE_LOG_INFO(...)  HUBBLE_LOG(HUBBLE_LOG_INFO, __VA_ARGS__)
#define HUBBLE_LOG_ERR(...)   HUBBLE_LOG(HUBBLE_LOG_ERR, __VA_ARGS__)

static const struct hubble_sat_api *sat_api;

int hubble_sat_init(void)
{
	sat_api = hubble_sat_api_get();

	if (sat_api == NULL || sat_api->transmit_packet == NULL) {
		return -ENOSYS;
	}

	HUBBLE_LOG_INFO("Hubble Satellite Network initialized\n");

	return 0;
}

int hubble_sat_power_set(int8_t power)
{
	if (sat_api == NULL || sat_api->power_set == NULL) {
		return -ENOSYS;
	}

	return sat_api->power_set(power);
}

int hubble_sat_channel_set(uint8_t channel)
{
	if (sat_api == NULL || sat_api->channel_set == NULL) {
		return -ENOSYS;
	}

	return sat_api->channel_set(channel);
}

int hubble_sat_enable(void)
{
	if (sat_api == NULL || sat_api->enable == NULL) {
		return -ENOSYS;
	}

	return sat_api->enable();
}

void hubble_sat_disable(void)
{
	if (sat_api == NULL || sat_api->disable == NULL) {
		return;
	}

	sat_api->disable();
}

int hubble_sat_transmit_packet(const struct hubble_sat_packet *packet)
{
	if (sat_api == NULL || sat_api->transmit_packet == NULL) {
		return -ENOSYS;
	}

	return sat_api->transmit_packet(packet);
}
