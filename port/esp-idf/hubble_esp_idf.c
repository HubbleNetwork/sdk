/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include "esp_log.h"

#include <hubble/port/sys.h>

#include "port_types.h"


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

static int _esp_to_errno(esp_err_t err)
{
	/* Lets adopt EIO as default error in the lack of something better */
	int ret = -EIO;

	switch (err) {
	case ESP_OK:
		ret = 0;
		break;
	case ESP_ERR_INVALID_ARG:
		ret = -EINVAL;
		break;
	case ESP_ERR_NO_MEM:
		ret = -ENOMEM;
		break;
	case ESP_ERR_INVALID_STATE:
		ret = -EALREADY;
		break;
	default:
		break;
	}

	return ret;
}

static void IRAM_ATTR _hubble_timer_cb(void *args)
{
	struct hubble_timer *h = args;

	h->cb(h, (void *)h->user_data);
}

int hubble_timer_init(struct hubble_timer *timer,
		      void (*cb)(struct hubble_timer *timer, void *user_data),
		      const void *user_data)
{
	if ((timer == NULL) || (cb == NULL)) {
		return -EINVAL;
	}

	timer->user_data = user_data;
	timer->cb = cb;

	const esp_timer_create_args_t timer_args = {
		.callback = _hubble_timer_cb,
		.arg = timer,
		.dispatch_method = ESP_TIMER_ISR,
	};

	return _esp_to_errno(esp_timer_create(&timer_args, &timer->esp_timer));
}

int hubble_timer_start(struct hubble_timer *timer, uint64_t expiration_us)
{
	esp_err_t err;

	if ((timer == NULL) || (expiration_us == UINT64_MAX)) {
		return -EINVAL;
	}

	if (esp_timer_is_active(timer->esp_timer)) {
		err = esp_timer_restart(timer->esp_timer, expiration_us);
	} else {
		err = esp_timer_start_periodic(timer->esp_timer, expiration_us);
	}

	return _esp_to_errno(err);
}

int hubble_timer_stop(struct hubble_timer *timer)
{
	esp_err_t err;

	if (timer == NULL) {
		return -EINVAL;
	}

	err = esp_timer_stop(timer->esp_timer);

	return _esp_to_errno(err);
}
