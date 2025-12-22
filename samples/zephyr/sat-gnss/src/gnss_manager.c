/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gnss.h>
#include <zephyr/sys/timeutil.h>

#include "gnss_manager.h"

#define GNSS_FIX_RATE_MS 30000U
#define GNSS_DEVICE      DEVICE_DT_GET(DT_NODELABEL(gnss))

static struct gnss_fix latest_fix;

K_SEM_DEFINE(fix_sem, 0, 1);
K_SEM_DEFINE(data_sem, 1, 1);

static void gnss_data_cb(const struct device *dev, const struct gnss_data *data)
{
	struct tm utc_time = {0};
	uint64_t utc_s;

	if (data->info.fix_status == GNSS_FIX_STATUS_NO_FIX) {
		return;
	}

	/*
	 * Convert and store the last GPS fix time
	 * GNSS API returns years since 2000
	 */
	utc_time.tm_year = data->utc.century_year + 100;
	utc_time.tm_mon = data->utc.month - 1;
	utc_time.tm_mday = data->utc.month_day;
	utc_time.tm_hour = data->utc.hour;
	utc_time.tm_min = data->utc.minute;
	utc_time.tm_sec = data->utc.millisecond / 1000;
	utc_s = timeutil_timegm(&utc_time);

	k_sem_take(&data_sem, K_FOREVER);
	latest_fix.latitude = data->nav_data.latitude / 1e9;
	latest_fix.longitude = data->nav_data.longitude / 1e9;
	latest_fix.utc = utc_s;
	latest_fix.uptime_ms = k_uptime_get();
	k_sem_give(&data_sem);

	k_sem_give(&fix_sem);
}

GNSS_DATA_CALLBACK_DEFINE(GNSS_DEVICE, gnss_data_cb);

int gnss_init(void)
{
	int err;

	err = gnss_set_fix_rate(GNSS_DEVICE, GNSS_FIX_RATE_MS);
	if (err != 0) {
		return err;
	}

	err = gnss_set_navigation_mode(GNSS_DEVICE,
				       GNSS_NAVIGATION_MODE_BALANCED_DYNAMICS);
	if (err != 0) {
		return err;
	}

	return err;
}

int gnss_get_fix(uint32_t timeout, struct gnss_fix *out)
{
	int err = 0;

	/* Drain the sem to make sure we get the lastest fix */
	while (k_sem_take(&fix_sem, K_NO_WAIT) == 0);
	err = k_sem_take(&fix_sem, (timeout == WAIT_FOREVER)
					   ? K_FOREVER
					   : K_SECONDS(timeout));

	if (err != 0) {
		return err;
	}

	k_sem_take(&data_sem, K_FOREVER);
	memset(out, 0, sizeof(*out));
	memcpy(out, &latest_fix, sizeof(*out));
	k_sem_give(&data_sem);

	return err;
}
