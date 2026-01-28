/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_random.h"

#include <hubble/port/sys.h>

int hubble_log(enum hubble_log_level level, const char *format, ...)
{
#if defined(CONFIG_LOG)
	static const char *_hubble_tag = "hubblenetwork";
	va_list args;
	static esp_log_level_t _log_level[HUBBLE_LOG_COUNT] = {
		[HUBBLE_LOG_DEBUG] = ESP_LOG_DEBUG,
		[HUBBLE_LOG_ERROR] = ESP_LOG_ERROR,
		[HUBBLE_LOG_INFO] = ESP_LOG_INFO,
		[HUBBLE_LOG_WARNING] = ESP_LOG_WARN,
	};

	va_start(args, format);
	esp_log_va(ESP_LOG_CONFIG_INIT(_log_level[level]), _hubble_tag, format,
		   args);
	va_end(args);
#endif /* defined(CONFIG_LOG) */

	return 0;
}

int hubble_rand_get(uint8_t *buffer, size_t len)
{
	esp_fill_random(buffer, len);
	return 0;
}
