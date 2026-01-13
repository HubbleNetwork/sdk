/*
 * Copyright (c) 2026 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <hubble/sat/packet.h>
#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>

#include "reed_solomon_encoder.h"
#include "utils/bitarray.h"
#include "utils/macros.h"

/* Number of bits to represent a device id */
#define HUBBLE_DEVICE_ID_SIZE       32

/* Number of bits to represent sequency number*/
#define HUBBLE_SEQUENCE_NUMBER_SIZE 10

/* Number of bits to represent authentication tag */
#define HUBBLE_AUTH_TAG_SIZE        32

#define HUBBLE_PHY_PROTOCOL_VERSION 0
#define HUBBLE_PHY_PROTOCOL_SIZE    6
#define HUBBLE_PHY_CHANNEL_SIZE     4
#define HUBBLE_PHY_PAYLOAD_SIZE     2

#define HUBBLE_PHY_ECC_SYMBOLS_SIZE 4
#define HUBBLE_PHY_SYMBOLS_SIZE     2

/* Number of bits to represent a symbol */
#define HUBBLE_SYMBOL_SIZE          6

#define HUBBLE_PAYLOAD_MAX_SIZE     13

static uint16_t _sequence_number;

static int _encode(const struct hubble_bitarray *bit_array, int *symbols,
		   size_t symbols_size)
{
	uint8_t symbol = 0;
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

static int8_t _packet_payload_size_get(size_t len)
{
	int8_t ret;

	switch (len) {
	case 0:
		ret = 13;
		break;
	case 4:
		ret = 18;
		break;
	case 9:
		ret = 25;
		break;
	case 13:
		ret = 30;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static uint8_t _phy_packet_size_get(size_t len)
{
	uint8_t ret;

	switch (len) {
	case 0:
		ret = 0b00;
		break;
	case 4:
		ret = 0b01;
		break;
	case 9:
		ret = 0b10;
		break;
	case 13:
		ret = 0b11;
		break;
	default:
		ret = 0b00;
		break;
	}

	return ret;
}

int hubble_sat_packet_get(struct hubble_sat_packet *packet, uint64_t device_id,
			  const void *payload, size_t length)
{
	int ret;
	struct hubble_bitarray bit_array;
	uint8_t ecc;
	int symbols[HUBBLE_PACKET_MAX_SIZE] = {0};
	int *rs_symbols;
	int payload_symbols_length;

	ret = hubble_rand_get(&packet->channel, sizeof(packet->channel));
	if (ret != 0) {
		packet->channel = 0;
		HUBBLE_LOG_WARNING("Could not pick a random channel");
	}

	packet->channel = packet->channel % HUBBLE_SAT_NUM_CHANNELS;

	payload_symbols_length = _packet_payload_size_get(length);
	if (payload_symbols_length < 0) {
		return -EINVAL;
	}

	hubble_bitarray_init(&bit_array);

	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){HUBBLE_PHY_PROTOCOL_VERSION},
		HUBBLE_PHY_PROTOCOL_SIZE);
	if (ret < 0) {
		return ret;
	}

	/* Let's encode physical frame (without preamble) */
	ret = hubble_bitarray_append(&bit_array, &packet->channel,
				     HUBBLE_PHY_CHANNEL_SIZE);
	if (ret < 0) {
		goto err;
	}

	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){_phy_packet_size_get(length)},
		HUBBLE_PHY_PAYLOAD_SIZE);
	if (ret < 0) {
		goto err;
	}

	ret = _encode(&bit_array, symbols, HUBBLE_PHY_SYMBOLS_SIZE);
	if (ret < 0) {
		goto err;
	}

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

	/* Sequence number */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&_sequence_number,
				     HUBBLE_SEQUENCE_NUMBER_SIZE);
	if (ret < 0) {
		goto err;
	}
	_sequence_number++;

	/* Device ID */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&device_id,
				     HUBBLE_DEVICE_ID_SIZE);
	if (ret < 0) {
		goto err;
	}

	/* Authentication tag */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&(uint32_t){0},
				     HUBBLE_AUTH_TAG_SIZE);
	if (ret < 0) {
		goto err;
	}

	/* Payload */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)payload,
				     length * HUBBLE_CHAR_BITS);
	if (ret < 0) {
		goto err;
	}

	/* This returns the number of symbols */
	ret = _encode(&bit_array, symbols, HUBBLE_PACKET_MAX_SIZE);
	if (ret < 0) {
		goto err;
	}

	for (uint8_t i = 0; i < payload_symbols_length; i++) {
		packet->data[packet->length + i] = symbols[i];
	}
	packet->length += payload_symbols_length;

	/* generate error control symbols */
	rse_gf_generate();
	ecc = _packet_payload_ecc_get(length) / 2;
	rse_poly_generate(ecc);
	rs_symbols = rse_rs_encode(symbols, ret, ecc);

	for (uint8_t i = 0; i < ecc * 2; i++) {
		packet->data[packet->length + i] = rs_symbols[i];
	}

	packet->length += (ecc * 2);

	return 0;

err:
	return ret;
}
