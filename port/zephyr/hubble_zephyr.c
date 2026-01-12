/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/random/random.h>
#include <zephyr/toolchain.h>

#include <errno.h>
#include <stddef.h>

#include <hubble/port/sys.h>

LOG_MODULE_REGISTER(hubblenetwork, CONFIG_HUBBLE_LOG_LEVEL);

uint64_t hubble_uptime_get(void)
{
	return (uint64_t)k_uptime_get();
}

__weak int hubble_log(enum hubble_log_level level, const char *format, ...)
{
#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_DEFAULT_MINIMAL)
	va_list args;
	static uint8_t zephyr_log_level[HUBBLE_LOG_COUNT] = {
		[HUBBLE_LOG_DEBUG] = LOG_LEVEL_DBG,
		[HUBBLE_LOG_ERROR] = LOG_LEVEL_ERR,
		[HUBBLE_LOG_INFO] = LOG_LEVEL_INF,
		[HUBBLE_LOG_WARNING] = LOG_LEVEL_WRN,
	};
	uint8_t zephyr_level = zephyr_log_level[level];

	if (zephyr_level > __log_level) {
		return 0;
	}

	va_start(args, format);
	z_log_msg_runtime_vcreate(0, __log_current_const_data, zephyr_level,
				  NULL, 0, 0, format, args);
	va_end(args);
#else
	ARG_UNUSED(level);
	ARG_UNUSED(format);
#endif /* defined(CONFIG_LOG) && !defined(CONFIG_LOG_DEFAULT_MINIMAL) */

	return 0;
}

int hubble_rand_get(uint8_t *buffer, size_t len)
{
	sys_rand_get(buffer, len);

	return 0;
}
