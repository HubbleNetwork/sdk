/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_ESP_IDF_BUILD
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <FreeRTOS.h>
#include <task.h>
#endif

#include <hubble/port/sys.h>

/**
 * Define weak attribute.
 *
 * This logic is scattered across all FreeRTOS code and modules.
 * Unfortunatelly it does not seems to have a common definition
 * for it so we need to define our own here.
 **/
#if defined(__CC_ARM)
#define HUBBLE_WEAK __attribute__((weak))
#elif defined(__ICCARM__)
#define HUBBLE_WEAK __weak
#elif defined(__GNUC__)
#define HUBBLE_WEAK __attribute__((weak))
#endif

uint64_t hubble_uptime_get(void)
{
	/*
	Ticks may wrap which we want to account for.

	NOTE: This is not thread safe and must be called only from the
	single context.

	NOTE: This will not be necessary if TickType_t is 64 bits wide,
	but we do not special case this as it will never wrap.
	*/
	static const unsigned BIT_IN_TICKS = sizeof((TickType_t)0) * 8;
	static uint64_t overflow_ticks = 0;
	static TickType_t last_ticks = 0;

	TickType_t this_ticks = xTaskGetTickCount();

	if (this_ticks < last_ticks) {
		overflow_ticks += ((uint64_t)1 << BIT_IN_TICKS);
	}
	last_ticks = this_ticks;

	return (((uint64_t)this_ticks + overflow_ticks) * 1000) /
	       configTICK_RATE_HZ;
}

HUBBLE_WEAK int hubble_log(enum hubble_log_level level, const char *format, ...)
{
	return 0;
}
