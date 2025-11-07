/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_AD_HELPER_H
#define INCLUDE_HUBBLE_AD_HELPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hubble BLE Advertisement Helper
 *
 * This API provides some optional helpers for simplifying creating, managing,
 * and configuring advertisements that adhere to the Hubble protocol. The use
 * of this API is purely optional and can be bypassed by power users who wish
 * to do more than simply advertise as a non-connectable device on the Hubble
 * protocol.
 *
 * Examples of more advanced usage may include:
 *  - Advertising additional service UUIDs
 *  - Advertising additional service data
 *  - Indicating a connectable device
 *
 * @defgroup hubble_ad_helper Advertisement Helper Functions
 * @since 0.1
 * @version 0.1
 * @{
 */

typedef struct {
	uint32_t interval_min;
	uint32_t interval_max;
} hubble_ad_config_t;

/**
 * @brief Initialize with the given configuration
 *
 * @param config Configuration data for the advertiser
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_init(const hubble_ad_config_t* config);

/**
 * @brief Update the configuration
 *
 * Advertising must be stopped before this is called.
 *
 * @param config Configuration data for the advertiser
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_update_config(const hubble_ad_config_t* config);

/**
 * @brief Deinitialize and clean up the adverising
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_deinit(void);

/**
 * @brief Start advertising
 *
 * If no user data has been set, this will advertise with no data.
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_start(void);

/**
 * @brief Stop advertising
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_stop(void);

/**
 * @brief Update the user data being advertised
 *
 * @return
 *          - 0 on success.
 *          - Non-zero on failure.
 */
int hubble_ad_update_data(const uint8_t *data, size_t len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_AD_HELPER_H */