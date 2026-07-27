/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hubble_priv.h"
#include "utils/hdcv.h"

#include <hubble/hubble.h>

#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

/* Currently, only support a rotation period of 86400 (daily) */
#if CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME &&                                  \
	(CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC != 86400)
#error Currently, only daily rotations are supported.
#endif

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
#define HUBBLE_EID_POOL_SIZE 128
#endif

static uint64_t unix_time_synced;
static uint64_t unix_time_base;

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
static uint64_t eid_initial_counter;
#endif

int hubble_time_set(uint64_t unix_time)
{
	uint64_t base = unix_time - hubble_uptime_get();
	if (base > unix_time) {
		return -EINVAL;
	}

	/* It holds when the device synced unix_time */
	unix_time_synced = unix_time;
	unix_time_base = base;

	return 0;
}

uint64_t hubble_time_get(void)
{
	return (unix_time_synced == 0) ? 0 : unix_time_base + hubble_uptime_get();
}

int hubble_init(uint64_t initial_time, const void *key)
{
	int ret = hubble_lock_init();

	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to initialize internal lock");
		return ret;
	}

	ret = hubble_crypto_init();
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to initialize cryptography");
		return ret;
	}

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
	/* Counter-based mode: initial_time is the starting counter value */
	/* 0 is a valid value meaning "start at epoch 0" */
	eid_initial_counter = initial_time;
#else
	/* Unix Epoch-based mode: initial_time is Unix Epoch timestamp in milliseconds */
	ret = hubble_time_set(initial_time);
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to set Unix Epoch time");
		return ret;
	}
#endif

	if (key != NULL) {
		ret = hubble_key_set(key);
		if (ret != 0) {
			HUBBLE_LOG_WARNING("Failed to set key");
			return ret;
		}
	}

#ifdef CONFIG_HUBBLE_SAT_NETWORK
	ret = hubble_internal_sat_init();
	if (ret != 0) {
		HUBBLE_LOG_ERROR(
			"Hubble Satellite Network initialization failed");
		return ret;
	}
#endif /* CONFIG_HUBBLE_SAT_NETWORK */

	HUBBLE_LOG_INFO("Hubble Device SDK initialized (%s)\n",
			HUBBLE_DEVICE_CONFIG_VECTOR);

	return 0;
}

uint64_t hubble_internal_time_last_synced_get(void)
{
	return unix_time_synced;
}

int hubble_counter_get(uint32_t *counter)
{
	if (counter == NULL) {
		return -EINVAL;
	}

	static const uint64_t rotation_period_ms =
		(uint64_t)CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC * 1000ULL;

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
	uint64_t uptime_ms = hubble_uptime_get();
	uint32_t uptime_epochs = (uint32_t)(uptime_ms / rotation_period_ms);
	/* Use uint64_t to prevent overflow before modulo */
	uint64_t total_counter = eid_initial_counter + uptime_epochs;

	*counter = (uint32_t)(total_counter % HUBBLE_EID_POOL_SIZE);
#else
	uint64_t unix_time = hubble_time_get();

	if (unix_time == 0U) {
		HUBBLE_LOG_WARNING("Unix Epoch time not initialized - call "
				   "hubble_time_set() or "
				   "hubble_init() with valid Unix time");
		return -EINVAL;
	}

	*counter = (uint32_t)(unix_time / rotation_period_ms);
#endif

	return 0;
}

const char *hubble_config_vector_get(void)
{
	return HUBBLE_DEVICE_CONFIG_VECTOR;
}

bool hubble_config_vector_check(const char *hdcv)
{
	if (hdcv == NULL) {
		return false;
	}

	return strcmp(HUBBLE_DEVICE_CONFIG_VECTOR, hdcv) == 0;
}
