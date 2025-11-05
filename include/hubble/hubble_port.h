/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_HUBBLE_PORT_H
#define INCLUDE_HUBBLE_HUBBLE_PORT_H

#include <stddef.h>
#include <stdint.h>

#include <hubble/sat/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hubble_log_level {
	HUBBLE_LOG_DEBUG,
	HUBBLE_LOG_INFO,
	HUBBLE_LOG_WARNING,
	HUBBLE_LOG_ERR,
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
#define HUBBLE_LOG_ERR(...)     HUBBLE_LOG(HUBBLE_LOG_ERR, __VA_ARGS__)
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
 * @brief Hubble Satellite Network porting API.
 * @defgroup hubble_sat_port Hubble Satellite Network porting APIs
 *
 * Functions and data structures that are used to port the Hubble Satellite
 * Network for different platforms.
 *
 * @{
 */
struct hubble_sat_api {
	/**
	 * @brief Transmits a packet.
	 *
	 * @param packet A pointer to a hubble_packet structure containing the
	 *               data to be transmitted.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*transmit_packet)(const struct hubble_sat_packet *packet);

	/**
	 * @brief Sets the transmission power
	 *
	 * Set the power that will be used when transmitting data.
	 *
	 * @param power The desired power level as an 8-bit signed integer
	 *              (~int8_t~). The valid range of values depends on the
	 *              specific implementation.
	 *
	 * @return 0 on success, non-zero on error.
	 *
	 */
	int (*power_set)(int8_t power);

	/**
	 * @brief Sets the channel
	 *
	 * Set the channel that will be used when transmitting data.
	 *
	 * @param channel The desired communication channel.
	 *                The valid range of values depends on the specific implementation.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*channel_set)(uint8_t channel);

	/**
	 * @brief Enables Hubble Network satellite stack.
	 *
	 * Setup everything that is need to transmit data.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*enable)(void);

	/**
	 * @brief Disables Hubble Network satellite stack.
	 *
	 * This API disables the satellite stack and restore devices to the
	 * state they were before.
	 *
	 * @return 0 on success, non-zero on error.
	 */
	int (*disable)(void);
};

/**
 * @brief Retrieves the satellite API interface.
 *
 * This function provides access to the satellite API, which includes logging
 * and packet transmission functionalities. The API is typically implemented
 * as a singleton.
 *
 * @return A pointer to a constant struct hubble_sat_api.
 *         Returns NULL if the API is unavailable.
 */
const struct hubble_sat_api *hubble_sat_api_get(void);

/** @} */ /* hubble_sat_port */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_HUBBLE_PORT_H */
