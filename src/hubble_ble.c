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

#include <hubble/ble.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

#include "hubble_priv.h"
#include "utils/macros.h"

#define BITS_PER_BYTE               8

#define HUBBLE_BLE_CONTEXT_LEN      12
#define HUBBLE_BLE_MESSAGE_LEN      64
#define HUBBLE_BLE_AUTH_LEN         16
#define HUBBLE_BLE_ADVERTISE_PREFIX 2
#define HUBBLE_BLE_PROTOCOL_VERSION 0b000000
#define HUBBLE_BLE_ADDR_SIZE        6
#define HUBBLE_BLE_AUTH_TAG_SIZE    4
#define HUBBLE_BLE_NONCE_LEN        12
#define HUBBLE_BLE_ADV_FIELDS_SIZE                                             \
	(HUBBLE_BLE_ADVERTISE_PREFIX + HUBBLE_BLE_ADDR_SIZE +                  \
	 HUBBLE_BLE_AUTH_TAG_SIZE)

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

/* Define some helpers for payload offsets */
#define _PAYLOAD_SERVICE_UUID_LO(buf) (buf + 0)
#define _PAYLOAD_SERVICE_UUID_HI(buf) (buf + 1)
#define _PAYLOAD_ADDR(buf)            (buf + HUBBLE_BLE_ADVERTISE_PREFIX)
#define _PAYLOAD_AUTH_TAG(buf)        ((_PAYLOAD_ADDR(buf)) + HUBBLE_BLE_ADDR_SIZE)
#define _PAYLOAD_DATA(buf)            ((_PAYLOAD_AUTH_TAG(buf)) + HUBBLE_BLE_AUTH_TAG_SIZE)

#ifndef CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM
uint16_t hubble_sequence_counter_get(void)
{
	/* Sequence number used to rotate keys */
	static uint16_t _sequence_number = 0U;

	if (_sequence_number > HUBBLE_BLE_MAX_SEQ_COUNTER) {
		_sequence_number = 0U;
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
	if ((_check_time_counter == 0U) || (_check_time_counter != time_counter)) {
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
	    (_check_seq_no_wrapped && (seq_no >= _check_seq_daily_reference_no))) {
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

static int _kbkdf_counter(const uint8_t *key, const char *label,
			  size_t label_len, const uint8_t *context,
			  size_t context_len, uint8_t *output, size_t olen)
{
	int ret = 0;
	uint8_t prf_output[HUBBLE_AES_BLOCK_SIZE];
	uint8_t message[HUBBLE_BLE_MESSAGE_LEN];
	uint32_t counter = 1U;
	uint32_t total = 0U;
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
		ret = hubble_crypto_cmac(key, message, message_length,
					 prf_output);
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
	hubble_crypto_zeroize(prf_output, sizeof(prf_output));
	hubble_crypto_zeroize(message, sizeof(message));

	return ret;
}

static int _derived_key_get(enum hubble_ble_key_label label, uint32_t counter,
			    uint8_t output_key[CONFIG_HUBBLE_KEY_SIZE])
{
	int err = 0;
	uint8_t context[HUBBLE_BLE_CONTEXT_LEN] = {0};
	const void *master_key = hubble_internal_key_get();

	snprintf((char *)context, HUBBLE_BLE_CONTEXT_LEN, "%" PRIu32, counter);

	switch (label) {
	case HUBBLE_BLE_DEVICE_KEY:
		err = _kbkdf_counter(master_key, "DeviceKey", strlen("DeviceKey"),
				     context, strlen((const char *)context),
				     output_key, CONFIG_HUBBLE_KEY_SIZE);
		break;
	case HUBBLE_BLE_NONCE_KEY:
		err = _kbkdf_counter(master_key, "NonceKey", strlen("NonceKey"),
				     context, strlen((const char *)context),
				     output_key, CONFIG_HUBBLE_KEY_SIZE);
		break;
	case HUBBLE_BLE_ENCRYPTION_KEY:
		err = _kbkdf_counter(master_key, "EncryptionKey",
				     strlen("EncryptionKey"), context,
				     strlen((const char *)context), output_key,
				     CONFIG_HUBBLE_KEY_SIZE);
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
	uint8_t derived_key[CONFIG_HUBBLE_KEY_SIZE] = {0};

	snprintf((char *)context, HUBBLE_BLE_CONTEXT_LEN, "%u", seq_no);

	switch (label) {
	case HUBBLE_BLE_DEVICE_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_DEVICE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "DeviceID", strlen("DeviceID"),
				     context, strlen((const char *)context),
				     output_value, output_len);
		break;
	case HUBBLE_BLE_NONCE_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_NONCE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "Nonce", strlen("Nonce"),
				     context, strlen((const char *)context),
				     output_value, output_len);
		break;
	case HUBBLE_BLE_ENCRYPTION_VALUE:
		ret = _derived_key_get(HUBBLE_BLE_ENCRYPTION_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "Key", strlen("Key"), context,
				     strlen((const char *)context),
				     output_value, output_len);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	hubble_crypto_zeroize(derived_key, sizeof(derived_key));
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

int hubble_ble_advertise_get(const uint8_t *input, size_t input_len,
			     uint8_t *out, size_t *out_len)
{
	int err;
	uint32_t device_id;
	uint32_t time_counter =
		hubble_internal_utc_time_get() / HUBBLE_TIMER_COUNTER_FREQUENCY;
	uint8_t encryption_key[CONFIG_HUBBLE_KEY_SIZE] = {0};
	uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN] = {0};
	uint8_t auth_tag[HUBBLE_BLE_AUTH_LEN] = {0};
	uint16_t seq_no;
	const void *master_key = hubble_internal_key_get();

	if ((master_key == NULL) || (out == NULL) || (out_len == NULL)) {
		return -EINVAL;
	}

	if ((input == NULL) && (input_len > 0)) {
		return -EINVAL;
	}

	if (input_len > HUBBLE_BLE_MAX_DATA_LEN) {
		return -EINVAL;
	}

	if (input_len + HUBBLE_BLE_ADV_FIELDS_SIZE > *out_len) {
		return -EINVAL;
	}

	seq_no = hubble_sequence_counter_get();

	if (!_nonce_values_check(time_counter, seq_no)) {
		HUBBLE_LOG_WARNING("Re-using same nonce is insecure !");
		return -EPERM;
	}

	// Set the constant data
	*_PAYLOAD_SERVICE_UUID_LO(out) = HUBBLE_LO_UINT16(HUBBLE_BLE_UUID);
	*_PAYLOAD_SERVICE_UUID_HI(out) = HUBBLE_HI_UINT16(HUBBLE_BLE_UUID);

	err = _derived_value_get(HUBBLE_BLE_DEVICE_VALUE, time_counter, 0,
				 (uint8_t *)&device_id, sizeof(device_id));
	if (err) {
		return err;
	}
	_addr_set(_PAYLOAD_ADDR(out), seq_no, device_id);

	err = _derived_value_get(HUBBLE_BLE_NONCE_VALUE, time_counter, seq_no,
				 nonce_counter, HUBBLE_BLE_NONCE_LEN);
	if (err) {
		goto err;
	}

	err = _derived_value_get(HUBBLE_BLE_ENCRYPTION_VALUE, time_counter,
				 seq_no, encryption_key, sizeof(encryption_key));
	if (err) {
		goto encryption_key_err;
	}

	err = hubble_crypto_aes_ctr(encryption_key, nonce_counter, input,
				    input_len, _PAYLOAD_DATA(out));
	if (err != 0) {
		goto crypt_ctr_err;
	}

	err = hubble_crypto_cmac(encryption_key, _PAYLOAD_DATA(out), input_len,
				 auth_tag);
	if (err != 0) {
		goto cmac_err;
	}

	memcpy(_PAYLOAD_AUTH_TAG(out), auth_tag, HUBBLE_BLE_AUTH_TAG_SIZE);

	*out_len = HUBBLE_BLE_ADVERTISE_PREFIX + HUBBLE_BLE_ADDR_SIZE +
		   HUBBLE_BLE_AUTH_TAG_SIZE + input_len;

cmac_err:
	hubble_crypto_zeroize(auth_tag, sizeof(auth_tag));
crypt_ctr_err:
	hubble_crypto_zeroize(encryption_key, sizeof(encryption_key));
encryption_key_err:
	hubble_crypto_zeroize(nonce_counter, sizeof(nonce_counter));
err:

	return err;
}
