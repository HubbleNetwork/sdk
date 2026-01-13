/*
 * Copyright (c) 2026 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <hubble/sat/packet.h>
#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>

#include "reed_solomon_encoder.h"
#include "utils/bitarray.h"
#include "utils/macros.h"

/* Number of bits to represent authentication tag */
#define HUBBLE_AUTH_TAG_SIZE                 32U

#define HUBBLE_PHY_PROTOCOL_VERSION          1U
#define HUBBLE_PHY_PROTOCOL_SIZE             4U
#define HUBBLE_PHY_HOP_INFO_SIZE             2U
#define HUBBLE_PHY_CHANNEL_SIZE              4U
#define HUBBLE_PHY_PAYLOAD_SIZE              2U

#define HUBBLE_PHY_ECC_SYMBOLS_SIZE          4U
#define HUBBLE_PHY_SYMBOLS_SIZE              2U

/* Number of bits to represent a symbol */
#define HUBBLE_SYMBOL_SIZE                   6U

#define HUBBLE_PAYLOAD_PROTOCOL_VERSION      0U
#define HUBBLE_PAYLOAD_PROTOCOL_VERSION_SIZE 2U
/* Number of bits to represent a device id */
#define HUBBLE_DEVICE_ID_SIZE                32U

/* Number of bits to represent sequency number*/
#define HUBBLE_SEQUENCE_NUMBER_SIZE          10U

#define HUBBLE_PAYLOAD_MAX_SIZE              13U

#define HUBBLE_SAT_CHANNEL_DEFAULT           5U

static uint16_t _sequence_number;

static int _encode(const struct hubble_bitarray *bit_array, int *symbols,
		   size_t symbols_size)
{
	uint8_t symbol = 0U;
	int symbol_bit_index = 0;
	int index = 0;

	if ((bit_array->index / HUBBLE_SYMBOL_SIZE) > symbols_size) {
		return -EINVAL;
	}

	for (size_t i = 0; i < bit_array->index; i++) {
		symbol |= (bit_array->data[i / 8] >> (i % 8) & 1)
			  << ((HUBBLE_SYMBOL_SIZE - symbol_bit_index - 1) %
			      HUBBLE_SYMBOL_SIZE);
		symbol_bit_index++;
		if ((i + 1) % HUBBLE_SYMBOL_SIZE == 0) {
			symbols[index] = symbol;
			symbol = 0;
			symbol_bit_index = 0;
			index++;
		}
	}

	/* Additional padding needed. */
	if ((bit_array->index % 6) > 0) {
		symbols[index++] = symbol;
	}

	return index;
}

static int _packet_payload_ecc_get(size_t len)
{
	int ecc;

	switch (len) {
	case 0:
		ecc = 10;
		break;
	case 4:
		ecc = 12;
		break;
	case 9:
		ecc = 14;
		break;
	case 13:
		ecc = 16;
		break;
	default:
		ecc = -1;
		break;
	}

	return ecc;
}

static int8_t _packet_payload_size_get(size_t len, uint8_t *length,
				       uint8_t *symbol)
{
	switch (len) {
	case 0:
		*length = 13;
		*symbol = 0b00;
		break;
	case 4:
		*length = 18;
		*symbol = 0b01;
		break;
	case 9:
		*length = 25;
		*symbol = 0b10;
		break;
	case 13:
		*length = 30;
		*symbol = 0b11;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int _whitening(uint8_t seed, int *symbols, size_t len)
{
	uint8_t state;
	size_t symbols_idx = 0U;
	uint8_t symbol_state = 0U;

	seed = 0x40 | seed;
	state = (3 << 5) | seed;

	for (int i = 0; i < len * 6; i++) {
		int fb;

		/*pack 6 bits into one symbol (MSB-first within the 6-bit symbol) */
		symbol_state |= (((state & 0x40) >> 6) << (5 - (i % 6)));

		/* Every six bits we have to XOR */
		if ((i % 6) == 5) {
			symbols[symbols_idx++] ^= symbol_state;
			symbol_state = 0;
		}

		fb = ((state >> 6U) ^ (state >> 3U)) & 1U;
		state = ((state << 1) & 0x7FU) | fb;
	}

	return 0;
}

int hubble_sat_packet_get(struct hubble_sat_packet *packet, uint64_t device_id,
			  const void *payload, size_t length)
{
	int ret;
	struct hubble_bitarray bit_array;
	uint8_t ecc;
	int symbols[HUBBLE_PACKET_MAX_SIZE] = {0};
	int *rs_symbols;
	uint8_t payload_symbols_length, payload_length_symbol, channel;

	if (hubble_rand_get(&channel, sizeof(channel))) {
		packet->channel = HUBBLE_SAT_CHANNEL_DEFAULT;
		HUBBLE_LOG_WARNING("Could not pick a random channel");
	} else {
		packet->channel = channel % HUBBLE_SAT_NUM_CHANNELS;
	}

	packet->hopping_sequence = channel % (1U << HUBBLE_PHY_HOP_INFO_SIZE);

	if (_packet_payload_size_get(length, &payload_symbols_length,
				     &payload_length_symbol) < 0) {
		return -EINVAL;
	}

	/* Let's encode physical frame (without preamble) */
	hubble_bitarray_init(&bit_array);

#define _CHECK_RET(_ret)                                                       \
	if (_ret < 0) {                                                        \
		return _ret;                                                   \
	}

	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){HUBBLE_PHY_PROTOCOL_VERSION},
		HUBBLE_PHY_PROTOCOL_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(&bit_array, &payload_length_symbol,
				     HUBBLE_PHY_PAYLOAD_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){packet->hopping_sequence},
		HUBBLE_PHY_HOP_INFO_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(&bit_array,
				     (uint8_t *)&(uint8_t){packet->channel},
				     HUBBLE_PHY_CHANNEL_SIZE);
	_CHECK_RET(ret);

	ret = _encode(&bit_array, symbols, HUBBLE_PHY_SYMBOLS_SIZE);
	_CHECK_RET(ret);

	for (uint8_t i = 0; i < HUBBLE_PHY_SYMBOLS_SIZE; i++) {
		packet->data[i] = symbols[i];
	}
	packet->length = HUBBLE_PHY_SYMBOLS_SIZE;

	rse_gf_generate();
	rse_poly_generate(HUBBLE_PHY_ECC_SYMBOLS_SIZE / 2);
	rs_symbols = rse_rs_encode(symbols, HUBBLE_PHY_SYMBOLS_SIZE,
				   HUBBLE_PHY_ECC_SYMBOLS_SIZE / 2);

	for (uint8_t i = 0; i < HUBBLE_PHY_ECC_SYMBOLS_SIZE; i++) {
		packet->data[i + packet->length] = rs_symbols[i];
	}
	packet->length += HUBBLE_PHY_ECC_SYMBOLS_SIZE;

	/* End of physical frame */

	/* Packet payload now. */
	hubble_bitarray_init(&bit_array);

	/* Payload version */
	ret = hubble_bitarray_append(
		&bit_array,
		(uint8_t *)&(uint8_t){HUBBLE_PAYLOAD_PROTOCOL_VERSION},
		HUBBLE_PAYLOAD_PROTOCOL_VERSION_SIZE);
	_CHECK_RET(ret);

	/* Sequence number */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&_sequence_number,
				     HUBBLE_SEQUENCE_NUMBER_SIZE);
	_CHECK_RET(ret);

	_sequence_number++;

	/* Device ID */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&device_id,
				     HUBBLE_DEVICE_ID_SIZE);
	_CHECK_RET(ret);

	/* Authentication tag */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&(uint32_t){0},
				     HUBBLE_AUTH_TAG_SIZE);
	_CHECK_RET(ret);

	/* Payload */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)payload,
				     length * HUBBLE_CHAR_BITS);
	_CHECK_RET(ret);

	/* This returns the number of symbols */
	ret = _encode(&bit_array, symbols, HUBBLE_PACKET_MAX_SIZE);
	_CHECK_RET(ret);

	/* generate error control symbols */
	rse_gf_generate();
	ecc = _packet_payload_ecc_get(length);
	rse_poly_generate(ecc / 2);
	rs_symbols = rse_rs_encode(symbols, ret, ecc / 2);

	/* We need to append rs_symbols to symbols before whitening them
	 * due lfsr7 state.
	 */
	memcpy(&symbols[ret], rs_symbols, ecc * sizeof(int));

	/* data whitening symbols before add them to the packet */
	ret = _whitening(packet->channel, symbols, ret + ecc);
	_CHECK_RET(ret);

	for (uint8_t i = 0; i < payload_symbols_length + ecc; i++) {
		packet->data[packet->length + i] = symbols[i];
	}
	packet->length += payload_symbols_length + ecc;

#undef _CHECK_RET

	return 0;
}
