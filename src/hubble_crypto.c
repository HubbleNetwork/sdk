/*
 * Copyright (c) 2026 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hubble/hubble.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

#include "hubble_priv.h"
#include "utils/macros.h"


#define _MESSAGE_SIZE  64U
#define _CONTEXT_SIZE  12U
#define _NONCE_SIZE    12U
#define _AUTH_TAG_SIZE 16U

enum hubble_key_label {
	HUBBLE_DEVICE_KEY,
	HUBBLE_NONCE_KEY,
	HUBBLE_ENCRYPTION_KEY
};

enum hubble_value_label {
	HUBBLE_DEVICE_VALUE,
	HUBBLE_NONCE_VALUE,
	HUBBLE_ENCRYPTION_VALUE
};

static const void *master_key;

const void *hubble_internal_key_get(void)
{
	return master_key;
}

int hubble_key_set(const void *key)
{
	if (key == NULL) {
		return -EINVAL;
	}

	master_key = key;

	return 0;
}

#ifndef CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM
uint16_t hubble_sequence_counter_get(void)
{
	/* Sequence number used to rotate keys */
	static uint16_t _sequence_number = 0U;

	if (_sequence_number > HUBBLE_MAX_SEQ_COUNTER) {
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
bool hubble_internal_nonce_values_check(uint32_t time_counter, uint16_t seq_no)
{
#ifdef CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK
	static bool _check_seq_no_wrapped;
	static uint32_t _check_time_counter;
	static uint16_t _check_seq_no;
	static uint16_t _check_seq_daily_reference_no;

	if (seq_no > HUBBLE_MAX_SEQ_COUNTER) {
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

int hubble_internal_sequence_acquire(uint32_t time_counter, uint16_t *seq_no)
{
	int ret = 0;

	/* Allocate the sequence number and validate the nonce as a single
	 * critical section. Doing both under the lock prevents two concurrent
	 * callers from being handed the same sequence number.
	 */
	hubble_lock();

	*seq_no = hubble_sequence_counter_get();
	if (!hubble_internal_nonce_values_check(time_counter, *seq_no)) {
		ret = -EPERM;
	}

	hubble_unlock();

	return ret;
}

static int _kbkdf_counter(const uint8_t *key, const char *label, size_t label_len,
			  bool hubble_key, const uint8_t *context,
			  size_t context_len, uint8_t *output, size_t olen)
{
	int ret = 0;
	uint8_t prf_output[HUBBLE_AES_BLOCK_SIZE];
	uint8_t message[_MESSAGE_SIZE];
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
	       (uint8_t *)&(uint32_t){
		       HUBBLE_CPU_TO_BE32(olen * HUBBLE_BITS_PER_BYTE)},
	       sizeof(uint32_t));

	while (total < olen) {
		size_t remaining = olen - total;

		/* Insert counter into the message */
		memcpy(message,
		       (uint8_t *)&(uint32_t){HUBBLE_CPU_TO_BE32(counter)},
		       sizeof(counter));

#ifdef CONFIG_HUBBLE_NETWORK_CRYPTO_PSA_USE_KEY_ID
		/* Perform AES-CMAC with the key and the prepared message */
		if (hubble_key) {
			ret = hubble_crypto_key_id_cmac(
				key, message, message_length, prf_output);
		} else {
			ret = hubble_crypto_cmac(key, message, message_length,
						 prf_output);
		}
#else
		(void)hubble_key;
		ret = hubble_crypto_cmac(key, message, message_length,
					 prf_output);
#endif

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

static int _derived_key_get(enum hubble_key_label label, uint32_t counter,
			    uint8_t output_key[CONFIG_HUBBLE_KEY_SIZE])
{
	int err = 0;
	uint8_t context[_CONTEXT_SIZE] = {0};

	snprintf((char *)context, _CONTEXT_SIZE, "%" PRIu32, counter);
	switch (label) {
	case HUBBLE_DEVICE_KEY:
		err = _kbkdf_counter(master_key, "DeviceKey",
				     strlen("DeviceKey"), true, context,
				     strlen((const char *)context), output_key,
				     CONFIG_HUBBLE_KEY_SIZE);
		break;
	case HUBBLE_NONCE_KEY:
		err = _kbkdf_counter(master_key, "NonceKey", strlen("NonceKey"),
				     true, context, strlen((const char *)context),
				     output_key, CONFIG_HUBBLE_KEY_SIZE);
		break;
	case HUBBLE_ENCRYPTION_KEY:
		err = _kbkdf_counter(master_key, "EncryptionKey",
				     strlen("EncryptionKey"), true, context,
				     strlen((const char *)context), output_key,
				     CONFIG_HUBBLE_KEY_SIZE);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int _derived_value_get(enum hubble_value_label label,
			      uint32_t time_counter, uint16_t seq_no,
			      uint8_t *output_value, uint32_t output_len)
{
	int ret = 0;
	uint8_t context[_CONTEXT_SIZE] = {0};
	uint8_t derived_key[CONFIG_HUBBLE_KEY_SIZE] = {0};

	snprintf((char *)context, _CONTEXT_SIZE, "%u", seq_no);

	switch (label) {
	case HUBBLE_DEVICE_VALUE:
		ret = _derived_key_get(HUBBLE_DEVICE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "DeviceID",
				     strlen("DeviceID"), false, context,
				     strlen((const char *)context),
				     output_value, output_len);
		break;
	case HUBBLE_NONCE_VALUE:
		ret = _derived_key_get(HUBBLE_NONCE_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(
			derived_key, "Nonce", strlen("Nonce"), false, context,
			strlen((const char *)context), output_value, output_len);
		break;
	case HUBBLE_ENCRYPTION_VALUE:
		ret = _derived_key_get(HUBBLE_ENCRYPTION_KEY, time_counter,
				       derived_key);
		if (ret != 0) {
			goto exit;
		}
		ret = _kbkdf_counter(derived_key, "Key", strlen("Key"), false,
				     context, strlen((const char *)context),
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

int hubble_internal_device_id_get(uint8_t *device_id, size_t device_id_len,
				  uint32_t counter)
{
	return _derived_value_get(HUBBLE_DEVICE_VALUE, counter, 0, device_id,
				  device_id_len);
}

int hubble_internal_data_encrypt(uint32_t counter, uint16_t seq_no,
				 const uint8_t *input, size_t input_len,
				 uint8_t *out, uint8_t *tag, size_t tag_len)
{
	int err;
	uint8_t auth_tag[_AUTH_TAG_SIZE] = {0};
	uint8_t encryption_key[CONFIG_HUBBLE_KEY_SIZE] = {0};
	uint8_t nonce_counter[HUBBLE_NONCE_BUFFER_SIZE] = {0};

	err = _derived_value_get(HUBBLE_NONCE_VALUE, counter, seq_no,
				 nonce_counter, _NONCE_SIZE);
	if (err) {
		goto err;
	}

	err = _derived_value_get(HUBBLE_ENCRYPTION_VALUE, counter, seq_no,
				 encryption_key, sizeof(encryption_key));
	if (err) {
		goto encryption_key_err;
	}

	err = hubble_crypto_aes_ctr(encryption_key, nonce_counter, input,
				    input_len, out);
	if (err != 0) {
		goto crypt_ctr_err;
	}

	err = hubble_crypto_cmac(encryption_key, out, input_len, auth_tag);
	if (err != 0) {
		goto cmac_err;
	}

	memcpy(tag, auth_tag, tag_len);

cmac_err:
	hubble_crypto_zeroize(auth_tag, sizeof(auth_tag));
crypt_ctr_err:
	hubble_crypto_zeroize(encryption_key, sizeof(encryption_key));
encryption_key_err:
	hubble_crypto_zeroize(nonce_counter, sizeof(nonce_counter));
err:
	return err;
}
