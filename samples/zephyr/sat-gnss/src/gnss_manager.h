/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GNSS_MANAGER_H
#define GNSS_MANAGER_H

#include <stdint.h>

#define WAIT_FOREVER UINT32_MAX

struct gnss_fix {
	double latitude;
	double longitude;
	uint64_t utc;
	uint64_t uptime_ms;
};

/**
 * Initialize the GNSS module.
 */
int gnss_init(void);

/**
 * Get the latest GNSS fix, waiting up to timeout seconds.
 *
 * @param timeout Maximum time to wait for a fix, in seconds.
 *                Use WAIT_FOREVER to wait indefinitely.
 * @param out Pointer to a struct gnss_fix to be filled with the latest fix data.
 *
 * @return 0 on success, negative error code on failure or timeout.
 */
int gnss_get_fix(uint32_t timeout, struct gnss_fix *out);

#endif /* GNSS_MANAGER_H */
