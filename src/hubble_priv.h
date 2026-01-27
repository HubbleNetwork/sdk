/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_HUBBLE_PRIV_H
#define SRC_HUBBLE_PRIV_H

#include <stdint.h>

const void *hubble_internal_key_get(void);

uint64_t hubble_internal_utc_time_get(void);

/* Returns the last time UTC was synced. It
 * is used to accommodate clock drifts.
 */
uint64_t hubble_internal_utc_time_last_synced_get(void);

uint32_t hubble_internal_eid_initial_counter_get(void);

#endif /* SRC_HUBBLE_PRIV_H */
