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
#if (LOG_LOCAL_LEVEL > ESP_LOG_NONE)
	static const char *_hubble_tag = "hubblenetwork";
	static const esp_log_level_t _log_level[HUBBLE_LOG_COUNT] = {
		[HUBBLE_LOG_DEBUG] = ESP_LOG_DEBUG,
		[HUBBLE_LOG_ERROR] = ESP_LOG_ERROR,
		[HUBBLE_LOG_INFO] = ESP_LOG_INFO,
		[HUBBLE_LOG_WARNING] = ESP_LOG_WARN,
	};

	/*
	 * require_formatting: add the level name, timestamp, tag, etc.
	 * This is Log V2 feature only, V1 just builds a plain string.
	 */
	esp_log_config_t config = {
		.opts = {
			.log_level = _log_level[level],
			.constrained_env = false,
			.require_formatting = true,
			.dis_color = ESP_LOG_COLOR_DISABLED,
			.dis_timestamp = ESP_LOG_TIMESTAMP_DISABLED,
			.reserved = 0,
		}};

	va_list args;

	va_start(args, format);
	esp_log_va(config, _hubble_tag, format, args);
	va_end(args);

#else
	(void)level;
	(void)format;
#endif /* LOG_LOCAL_LEVEL > ESP_LOG_NONE */

	return 0;
}

int hubble_rand_get(uint8_t *buffer, size_t len)
{
	esp_fill_random(buffer, len);
	return 0;
}
