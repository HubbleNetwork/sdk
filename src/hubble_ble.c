/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hubble_ble.h>
#include <hubble_port.h>

#include "utils/macros.h"

#define BITS_PER_BYTE 8

static uint64_t utc_time_base;
static const struct hubble_ble_api *ble_api;
static const void *master_key;

#define HUBBLE_BLE_CONTEXT_LEN          12
#define HUBBLE_BLE_MESSAGE_LEN          64
#define HUBBLE_BLE_AUTH_LEN             16
#define HUBBLE_BLE_ADVERTISE_PREFIX     2
#define HUBBLE_BLE_PROTOCOL_VERSION     0b000000
#define HUBBLE_BLE_ADDR_SIZE            6
#define HUBBLE_BLE_AUTH_TAG_SIZE        4
#define HUBBLE_BLE_NONCE_LEN            12
#define HUBBLE_BLE_ADV_FIELDS_SIZE      (HUBBLE_BLE_ADVERTISE_PREFIX + HUBBLE_BLE_ADDR_SIZE + HUBBLE_BLE_AUTH_TAG_SIZE)

#if defined(CONFIG_HUBBLE_BLE_NETWORK_TIMER_COUNTER_DAILY)
#define HUBBLE_TIMER_COUNTER_FREQUENCY 86400000
#else
#error "No valid TIMER COUNTER value"
#endif

enum hubble_ble_key_label {
	HUBBLE_BLE_DEVICE_KEY,
	HUBBLE_BLE_NONCE_KEY,
	HUBBLE_BLE_ENCRYPTION_KEY
};

enum hubble_ble_value_label {
	HUBBLE_BLE_DEVICE_VALUE,
	HUBBLE_BLE_NONCE_VALUE,
	HUBBLE_BLE_ENCRYPTION_VALUE
};

static uint8_t advertise_buffer[CONFIG_HUBBLE_BLE_ADVERTISE_BUFFER_SIZE] = {
	0xa6, 0xfc};

/* Define some helpers for payload offsets */
#define _PAYLOAD_ADDR     (advertise_buffer + HUBBLE_BLE_ADVERTISE_PREFIX)
#define _PAYLOAD_AUTH_TAG ((_PAYLOAD_ADDR) + HUBBLE_BLE_ADDR_SIZE)
#define _PAYLOAD_DATA     ((_PAYLOAD_AUTH_TAG) + HUBBLE_BLE_AUTH_TAG_SIZE)

#ifndef CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM
uint16_t hubble_sequence_counter_get(void)
{
	/* Sequence number used to rotate keys */
	static uint16_t _sequence_number = 0;

	if (_sequence_number > HUBBLE_BLE_MAX_SEQ_COUNTER) {
		_sequence_number = 0;
	}

	return _sequence_number++;
}
#endif /* CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM */

/**
 * Returns true if the time_counter and seq_no are unique (not reused)
 * or false otherwise.
 * This assumes that the sequence is incremental. Wrapping is allowed.
 */
static bool _nonce_values_check(uint32_t time_counter, uint16_t seq_no)
{
#ifdef CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK
	static bool _check_seq_no_wrapped;
	static uint32_t _check_time_counter;
	static uint16_t _check_seq_no;
	static uint16_t _check_seq_daily_reference_no;

	if (seq_no > HUBBLE_BLE_MAX_SEQ_COUNTER) {
		return false;
	}

	/* Time counter changed, any sequence number is valid.
	 * We just need to update our daily reference for checking for
	 * sequence wrapper.
	 */
	if ((_check_time_counter == 0) || (_check_time_counter != time_counter)) {
		_check_seq_daily_reference_no = seq_no;
		_check_seq_no_wrapped = false;
		_check_time_counter = time_counter;
		_check_seq_no = seq_no;
		return true;
	}

	/* We need to check if the sequence number is the same or
	 * if it wrapped we need to ensure that this is not bigger
	 * than the first value used with the current time counter.
	 */
	if ((_check_seq_no == seq_no) ||
	    (_check_seq_no_wrapped  && (seq_no >= _check_seq_daily_reference_no))) {
		return false;
	}

	/* At this point the sequence is not the same but we need to check if it just wrapped. */
	if (_check_seq_no > seq_no) {
		_check_seq_no_wrapped = true;
		/* That is the first sequence number after wrapping, lets ensure
		 * that it is not bigger than the daily reference.
		 */
		if (seq_no >= _check_seq_daily_reference_no) {
			return false;
		}
	}

	_check_seq_no = seq_no;

#endif /* CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK */
	return true;
}

int hubble_ble_init(uint64_t utc_time)
{
	ble_api = hubble_ble_api_get();

	if (ble_api == NULL) {
		return -ENOSYS;
	}

	HUBBLE_LOG_INFO("Hubble BLE Network initialized\n");

	utc_time_base = utc_time - hubble_uptime_get();

	return 0;
}

static int _kbkdf_counter(const uint8_t *key, const uint8_t *label,
			  size_t label_len, const uint8_t *context,
			  size_t context_len, uint8_t *output, size_t olen)
{
	int ret = 0;
	uint8_t prf_output[HUBBLE_AES_BLOCK_SIZE];
	uint8_t message[HUBBLE_BLE_MESSAGE_LEN];
	uint32_t counter = 1;
	uint32_t total = 0;
	uint8_t separation_byte = 0x00;

	/* Message format: Counter + Label + Context + Length (in bits) */
	uint32_t message_length = sizeof(counter) + label_len +
				  sizeof(separation_byte) + context_len +
				  sizeof(uint32_t);

	/* Check for message length overflow */
	if (message_length >= sizeof(message)) {
		ret = -EINVAL;
		goto exit;
	}

	/* Prepare the message with Label, Context, and bit size */

	/* Copy label after the counter */
	memcpy((message + sizeof(counter)), label, label_len);
	/* Separation byte (as defined by the standard) */
	message[sizeof(counter) + label_len] = separation_byte;
	/* Copy the context */
	memcpy((message + sizeof(counter) + label_len + sizeof(separation_byte)),
	       context, context_len);
	/* Length in bits at the end */
	memcpy((message + sizeof(counter) + label_len +
		sizeof(separation_byte) + context_len),
	       (uint8_t *)&(uint32_t){HUBBLE_CPU_TO_BE32(olen * BITS_PER_BYTE)},
	       sizeof(uint32_t));

	while (total < olen) {
		size_t remaining = olen - total;

		/* Insert counter into the message */
		memcpy(message,
		       (uint8_t *)&(uint32_t){HUBBLE_CPU_TO_BE32(counter)},
		       sizeof(counter));

		/* Perform AES-CMAC with the key and the prepared message */
		ret = ble_api->cmac(key, message, message_length, prf_output);
		if (ret != 0) {
			goto exit;
		}

		/* Copy the output */
		if (remaining > HUBBLE_AES_BLOCK_SIZE) {
			remaining = HUBBLE_AES_BLOCK_SIZE;
		}

		memcpy(output + total, prf_output, remaining);
		total += remaining;
		counter++;
	}

exit:
	/* Clear sensitive information */
	ble_api->zeroize(prf_output, sizeof(prf_output));
	ble_api->zeroize(message, sizeof(message));

	return ret;
}

static int _derived_key_get(enum hubble_ble_key_label label, uint32_t counter,
			    uint8_t output_key[HUBBLE_BLE_KEY_LEN])
{
	int err = 0;
	uint8_t context[HUBBLE_BLE_CONTEXT_LEN] = {0};

	snprintf((char *)context, HUBBLE_BLE_CONTEXT_LEN, "%" PRIu32, counter);

	switch (label) {
	case HUBBLE_BLE_DEVICE_KEY:
		err = _kbkdf_counter(master_key, "DeviceKey", strlen("DeviceKey"),
				     context, strlen(context), output_key,
				     HUBBLE_BLE_KEY_LEN);
		break;
	case HUBBLE_BLE_NONCE_KEY:
		err = _kbkdf_counter(master_key, "NonceKey", strlen("NonceKey"),
				     context, strlen(context), output_key,
				     HUBBLE_BLE_KEY_LEN);
		break;
	case HUBBLE_BLE_ENCRYPTION_KEY:
		err = _kbkdf_counter(master_key, "EncryptionKey",
				     strlen("EncryptionKey"), context,
				     strlen(context), output_key,
				     HUBBLE_BLE_KEY_LEN);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int _derived_value_get(enum hubble_ble_value_label label,
			      uint32_t time_counter, uint16_t seq_no,
			      uint8_t *output_value, uint32_t output_len)
{
	int ret = 0;
	uint8_t context[HUBBLE_BLE_CONTEXT_LEN] = {0};
	uint8_t derived_key[HUBBLE_BLE_KEY_LEN] = {0};

	snprintf((char *)context, HUBBLE_BLE_CONTEXT_LEN, "%u", seq_no);

	switch (label) {
	case HUBBLE_BLE_DEVICE_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_DEVICE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "DeviceID",
				     strlen("DeviceID"), context,
				     strlen(context), output_value, output_len);
		break;
	case HUBBLE_BLE_NONCE_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_NONCE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "Nonce", strlen("Nonce"),
				     context, strlen(context), output_value,
				     output_len);
		break;
	case HUBBLE_BLE_ENCRYPTION_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_ENCRYPTION_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "Key", strlen("Key"), context,
				     strlen(context), output_value, output_len);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	ble_api->zeroize(derived_key, sizeof(derived_key));
exit:
	return ret;
}

static void _addr_set(uint8_t *addr, uint16_t seq_no, uint32_t device_id)
{
	uint8_t seq_no_first_2bits = (seq_no >> 8) & 0x03;
	uint8_t seq_no_last_8bits = seq_no & 0xFF;

	addr[0] = HUBBLE_BLE_PROTOCOL_VERSION | seq_no_first_2bits;
	addr[1] = seq_no_last_8bits;

	memcpy((addr + 2), &device_id, sizeof(device_id));
}

void *hubble_ble_advertise_get(const uint8_t *data, size_t len, size_t *out_len)
{
	int err;
	uint32_t device_id;
	uint32_t time_counter = (utc_time_base + hubble_uptime_get()) /
				HUBBLE_TIMER_COUNTER_FREQUENCY;
	uint8_t encryption_key[HUBBLE_BLE_KEY_LEN] = {0};
	uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN] = {0};
	uint8_t auth_tag[HUBBLE_BLE_AUTH_LEN] = {0};
	uint16_t seq_no;

	if ((ble_api == NULL) || (master_key == NULL)) {
		return NULL;
	}

	if (len >= (sizeof(advertise_buffer) - HUBBLE_BLE_ADV_FIELDS_SIZE)) {
		return NULL;
	}

	seq_no = hubble_sequence_counter_get();

	if (!_nonce_values_check(time_counter, seq_no)) {
		HUBBLE_LOG_WARNING("Re-using same nonce is insecure !");
		return NULL;
	}

	memset(advertise_buffer + HUBBLE_BLE_ADVERTISE_PREFIX, 0,
	       sizeof(advertise_buffer) - HUBBLE_BLE_ADVERTISE_PREFIX);

	err = _derived_value_get(HUBBLE_BLE_DEVICE_VALUE, time_counter, 0,
				 (uint8_t *)&device_id, sizeof(device_id));
	if (err) {
		return NULL;
	}
	_addr_set(_PAYLOAD_ADDR, seq_no, device_id);

	err = _derived_value_get(HUBBLE_BLE_NONCE_VALUE, time_counter,
				 seq_no, nonce_counter,
				 HUBBLE_BLE_NONCE_LEN);
	if (err) {
		goto err;
	}

	err = _derived_value_get(HUBBLE_BLE_ENCRYPTION_VALUE, time_counter,
				 seq_no, encryption_key,
				 sizeof(encryption_key));
	if (err) {
		goto encryption_key_err;
	}

	err = ble_api->aes_ctr(encryption_key, (size_t){0}, nonce_counter, data,
			       len, _PAYLOAD_DATA);
	if (err != 0) {
		goto crypt_ctr_err;
	}

	err = ble_api->cmac(encryption_key, _PAYLOAD_DATA, len, auth_tag);
	if (err != 0) {
		goto cmac_err;
	}

	memcpy(_PAYLOAD_AUTH_TAG, auth_tag, HUBBLE_BLE_AUTH_TAG_SIZE);

	if (out_len) {
		*out_len = HUBBLE_BLE_ADVERTISE_PREFIX +
			   HUBBLE_BLE_ADDR_SIZE +
			   HUBBLE_BLE_AUTH_TAG_SIZE + len;
	}

cmac_err:
	ble_api->zeroize(auth_tag, sizeof(auth_tag));
crypt_ctr_err:
	ble_api->zeroize(encryption_key, sizeof(encryption_key));
encryption_key_err:
	ble_api->zeroize(nonce_counter, sizeof(nonce_counter));
err:

	return (err == 0) ? advertise_buffer : NULL;
}

int hubble_ble_utc_set(uint64_t utc_time)
{
	if (ble_api == NULL) {
		return -EINVAL;
	}

	utc_time_base = utc_time - hubble_uptime_get();

	return 0;
}

int hubble_ble_key_set(const void *key)
{
	if (key == NULL) {
		return -EINVAL;
	}

	master_key = key;

	return 0;
}
