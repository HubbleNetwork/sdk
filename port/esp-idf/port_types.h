/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file port_types.h
 * @brief ESP-IDF-specific type definitions for the Hubble Network SDK port
 *
 * This header file defines platform-specific types used by the Hubble Network
 * SDK when running on ESP-IDF. These types provide the concrete implementation
 * of the abstract interfaces defined in the SDK's port abstraction layer.
 *
 * @note This file is included by include/hubble/port/sys.h and should not be
 *       included directly by application code.
 */

#ifndef PORT_ESP_IDF_PORT_TYPES_H
#define PORT_ESP_IDF_PORT_TYPES_H

#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP-IDF-specific timer structure implementation
 *
 * This structure provides the ESP-IDF implementation of the abstract
 * hubble_timer type. It wraps the ESP-IDF timer (esp_timer_handle_t) and adds
 * callback function and user data fields required by the Hubble SDK timer API.
 */
struct hubble_timer {
	/** @brief Underlying ESP-IDF timer instance */
	esp_timer_handle_t esp_timer;

	/** @brief User-provided data pointer passed to the callback function */
	const void *user_data;

	/**
	 * @brief Callback function invoked when the timer expires
	 *
	 * This function is called from interrupt context when the timer expires.
	 * The callback receives a pointer to the timer that triggered it and the
	 * user_data pointer that was provided during initialization.
	 *
	 * @param t Pointer to the hubble_timer structure that expired
	 * @param user_data User data pointer provided during timer initialization
	 */
	void (*cb)(struct hubble_timer *t, void *user_data);
};

#ifdef __cplusplus
}
#endif

#endif /* PORT_ESP_IDF_PORT_TYPES_H */
