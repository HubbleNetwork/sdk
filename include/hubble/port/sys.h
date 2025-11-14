/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_PORT_SYS_H
#define INCLUDE_HUBBLE_PORT_SYS_H

#include <stddef.h>
#include <stdint.h>

/**
 * This header must be defined by each and included in the build
 * included path.
 *
 * The reason for this header is to avoid the need of heap to use
 * objects that are platform dependent, like struct hubble_timer.
 */
#include <port_types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hubble_log_level {
	HUBBLE_LOG_DEBUG,
	HUBBLE_LOG_INFO,
	HUBBLE_LOG_WARNING,
	HUBBLE_LOG_ERROR,
	/* Number of log levels (internal use) */
	HUBBLE_LOG_COUNT,
};

#define HUBBLE_BLE_NONCE_BUFFER_LEN 16
#define HUBBLE_AES_BLOCK_SIZE       16
/* Valid range [0, 1023] */
#define HUBBLE_BLE_MAX_SEQ_COUNTER  ((1 << 10) - 1)

#define HUBBLE_LOG(_level, ...)                                                \
	do {                                                                   \
		hubble_log((_level), __VA_ARGS__);                             \
	} while (0)

#define HUBBLE_LOG_DEBUG(...)   HUBBLE_LOG(HUBBLE_LOG_DEBUG, __VA_ARGS__)
#define HUBBLE_LOG_INFO(...)    HUBBLE_LOG(HUBBLE_LOG_INFO, __VA_ARGS__)
#define HUBBLE_LOG_WARNING(...) HUBBLE_LOG(HUBBLE_LOG_WARNING, __VA_ARGS__)
#define HUBBLE_LOG_ERROR(...)   HUBBLE_LOG(HUBBLE_LOG_ERROR, __VA_ARGS__)

#define HUBBLE_TIME_FOREVER     UINT64_MAX

/**
 * @brief Timer structure for periodic callback execution.
 *
 * The actual implementation of this structure is platform-dependent and is
 * defined in the port-specific header file (port_types.h). Applications should
 * treat this as an opaque type and only interact with it through the provided
 * timer API functions.
 */
struct hubble_timer;

/**
 * @brief Function pointer to retrieve the target system uptime.
 *
 * This function pointer, when called, returns the current uptime of the
 * target system. The uptime is measured in milliseconds since the
 * system was started.
 *
 * @return The current uptime of the target system in milliseconds.
 */
uint64_t hubble_uptime_get(void);

/**
 * @brief Retrieves the current sequence counter value for BLE advertising.
 *
 * This function returns the current sequence number used in the Hubble BLE
 * Network protocol. The sequence counter is a 10-bit value (0-1023) that
 * increments with each BLE advertisement and is used for:
 * - Key rotation and derivation
 * - Nonce generation for encryption
 * - BLE address generation
 * - Ensuring uniqueness of advertisements
 *
 * The sequence counter automatically wraps around to 0 when it reaches the
 * maximum value (1023).
 *
 * @note This function can be override by the application it with
 *       custom sequence counter logic defining the symbol
 *       `CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM`
 *
 * @return The current sequence counter value (0-1023).
 */
uint16_t hubble_sequence_counter_get(void);

/**
 * @brief Logs a message with a specified log level.
 *
 * This function logs a formatted message to the logging system with the
 * specified log level. The message format and additional arguments
 * follow the same conventions as printf.
 *
 * @param level The log level of the message. This should be one of the
 *              values defined in the `hubble_log_level` enum.
 * @param format The format string for the log message. This follows the
 *               same syntax as the printf format string.
 * @param ... Additional arguments for the format string.
 *
 * @return Returns 0 indicating success. Non-zero value otherwise.
 */
int hubble_log(enum hubble_log_level level, const char *format, ...);

/**
 * @brief Initializes a timer with a callback function.
 *
 * This function initializes a timer structure with the provided callback
 * function. The timer must be initialized before it can be started or stopped.
 * The callback function will be invoked when the timer expires.
 *
 * @note The callback is called from interruption context!
 *
 * @param timer Pointer to the timer structure to initialize. Must not be NULL.
 * @param cb Callback function to be called when the timer expires. The callback
 *           receives a pointer to the timer that triggered it.
 * @param user_data User data to be passed in the callback.
 *
 * @return Returns 0 indicating success. Non-zero value otherwise.
 */
int hubble_timer_init(struct hubble_timer *timer,
		      void (*cb)(struct hubble_timer *timer, void *user_data),
		      const void *user_data);

/**
 * @brief Starts a periodic timer with the specified expiration interval.
 *
 * This function starts a timer that will periodically expire at the specified
 * interval. The timer will fire repeatedly at the given expiration interval
 * until it is stopped. The callback function provided during initialization
 * will be called each time the timer expires.
 *
 * @param timer Pointer to the timer structure to start. Must not be NULL and
 *              must have been initialized with hubble_timer_init.
 * @param expiration_us The expiration interval in microseconds. This value
 *                      determines both the initial delay before the first
 *                      expiration and the period for subsequent expirations.
 *
 * @return Returns 0 indicating success. Non-zero value otherwise.
 */
int hubble_timer_start(struct hubble_timer *timer, uint64_t expiration_us);

/**
 * @brief Stops a running timer.
 *
 * This function stops a timer that is currently running. After stopping, the
 * timer will no longer fire and the callback function will not be called until
 * the timer is started again with hubble_timer_start.
 *
 * @param timer Pointer to the timer structure to stop. Must not be NULL.
 *
 * @return Returns 0 indicating success. Non-zero value otherwise.
 */
int hubble_timer_stop(struct hubble_timer *timer);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_PORT_SYS_H */
