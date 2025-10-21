/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <FreeRTOS.h>
#include <task.h>

#include <hubble/hubble_port.h>

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
	return ((uint64_t)xTaskGetTickCount() * 1000) / configTICK_RATE_HZ;
}

HUBBLE_WEAK int hubble_log(enum hubble_log_level level, const char *format, ...)
{
	return 0;
}
