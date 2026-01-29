/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

/* Compile-time validation of configuration */
#if CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC < 900 ||                             \
	CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC > 86400
#error "CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC must be between 900 and 86400 seconds"
#endif

#if CONFIG_HUBBLE_EID_COUNTER_BASED
#if CONFIG_HUBBLE_EID_COUNTER_BITS < 4 || CONFIG_HUBBLE_EID_COUNTER_BITS > 11
#error "CONFIG_HUBBLE_EID_COUNTER_BITS must be between 4 and 11"
#endif
#endif

static uint64_t utc_time_synced;
static uint64_t utc_time_base;
static const void *master_key;
static uint32_t eid_initial_counter;

int hubble_utc_set(uint64_t utc_time)
{
	if (utc_time == 0U) {
		return -EINVAL;
	}

	/* It holds when the device synced utc */
	utc_time_synced = utc_time;

	utc_time_base = utc_time - hubble_uptime_get();

	return 0;
}

int hubble_key_set(const void *key)
{
	if (key == NULL) {
		return -EINVAL;
	}

	master_key = key;

	return 0;
}

int hubble_init(uint64_t initial_time, const void *key)
{
	int ret = hubble_crypto_init();

	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to initialize cryptography");
		return ret;
	}

#ifdef CONFIG_HUBBLE_EID_COUNTER_BASED
	/* Counter-based mode: initial_time is the starting counter value */
	/* 0 is a valid value meaning "start at epoch 0" */
	eid_initial_counter = (uint32_t)initial_time;

	/* UTC time tracking is still available but optional */
	utc_time_base = 0;
#else
	/* UTC-based mode: initial_time is UTC timestamp in milliseconds */
	ret = hubble_utc_set(initial_time);
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to set UTC time");
		return ret;
	}
#endif

	ret = hubble_key_set(key);
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Failed to set key");
		return ret;
	}

#ifdef CONFIG_HUBBLE_SAT_NETWORK
	ret = hubble_sat_port_init();
	if (ret != 0) {
		HUBBLE_LOG_ERROR(
			"Hubble Satellite Network initialization failed");
		return ret;
	}
#endif /* CONFIG_HUBBLE_SAT_NETWORK */

	HUBBLE_LOG_INFO("Hubble Network SDK initialized\n");

	return 0;
}

const void *hubble_internal_key_get(void)
{
	return master_key;
}

uint64_t hubble_internal_utc_time_get(void)
{
	return utc_time_base + hubble_uptime_get();
}

uint64_t hubble_internal_utc_time_last_synced_get(void)
{
	return utc_time_synced;
}

uint32_t hubble_internal_eid_initial_counter_get(void)
{
	return eid_initial_counter;
}

uint32_t hubble_eid_counter_get(void)
{
	uint64_t rotation_period_ms =
		(uint64_t)CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC * 1000ULL;

#ifdef CONFIG_HUBBLE_EID_COUNTER_BASED
	uint64_t uptime_ms = hubble_uptime_get();
	uint32_t uptime_epochs = (uint32_t)(uptime_ms / rotation_period_ms);
	uint32_t total_counter = eid_initial_counter + uptime_epochs;

	return total_counter % (1U << CONFIG_HUBBLE_EID_COUNTER_BITS);
#else
	return (uint32_t)(hubble_internal_utc_time_get() / rotation_period_ms);
#endif
}
