/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Internal interface for board / soc
 *        support to implement satellite comms.
 * @file
 * @internal
 *
 * The interface in this file is internal and not stable.
 */

#ifndef PORT_ZEPHYR_HUBBLE_RF_ZEPHYR_H
#define PORT_ZEPHYR_HUBBLE_RF_ZEPHYR_H

/**
 * @brief Initializes the RF module.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_init(void);

/**
 * @brief Enable the RF module.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_enable(void);

/**
 * @brief Disable the RF module.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_disable(void);

/**
 * @brief Starts transmitting a continuous wave.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_cw_start(void);

/**
 * @brief Stops transmitting a continuous wave.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_cw_stop(void);

/**
 * @brief Sets the frequency step for the RF module.
 *
 * @param step The frequency step to set.
 *
 * @return 0 on success, negative error code on failure.
 */
int hubble_rf_frequency_step_set(uint16_t step);


#endif /* PORT_ZEPHYR_HUBBLE_RF_ZEPHYR_H */
