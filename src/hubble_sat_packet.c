#include <errno.h>
#include <math.h>
#include <stdbool.h>

#include <hubble/sat/packet.h>

#include "reed_solomon_encoder.h"
#include "utils/bitarray.h"
#include "utils/macros.h"

/* Number of bits to represent a device id */
#define HUBBLE_DEVICE_ID_SIZE          34

/* Number of bits to represent sequency number*/
#define HUBBLE_SEQUENCE_NUMBER_SIZE    10

/* Number of bits to represent authentication tag */
#define HUBBLE_AUTH_TAG_SIZE           16

/* device id (34 bits) + auth tag (16 bits) + sequency number (16 bits)*/
#define HUBBLE_MAC_HEADER_SYMBOLS_SIZE 10

/* Number of symbols to indicate the packet length. @Note: the length
 * is represented in only one symbol but we replicate this information
 * across the packet the number of times.
 */
#define HUBBLE_MAC_LENGTH_SYMBOLS      3

#define HUBBLE_MAC_LENGTH_MASK         0x1F

/* Number of bits to represent a symbol */
#define HUBBLE_SYMBOL_SIZE             6

#define HUBBLE_PACKET_FRAME_MAX_SIZE   25

static const uint8_t _hubble_mac_frame_symbols[] = {
	11, 13, 15, 17, 19, 21, 23, 25
};
static const uint8_t _hubble_mac_error_control_symbols[] = {
	10, 10, 12, 12, 14, 14, 16, 16
};
static const uint8_t _hubble_packet_total_symbols[] = {
	24, 26, 30, 32, 36, 38, 42, 44
};

static uint16_t _sequence_number;

/* Returns the index (_hubble_packet_total_symbols) to the total number of
 * symbols needed for the packet.
 **/
static int _mac_total_symbols_index_get(size_t number_of_symbols)
{
	size_t idx;

	for (idx = 0; idx < HUBBLE_ARRAY_SIZE(_hubble_mac_frame_symbols); idx++) {
		if (number_of_symbols <= _hubble_mac_frame_symbols[idx]) {
			break;
		}
	}

	if (idx < HUBBLE_ARRAY_SIZE(_hubble_mac_frame_symbols)) {
		return idx;
	}

	return -EINVAL;
}

static bool _payload_length_check(size_t length)
{
	uint8_t idx = HUBBLE_ARRAY_SIZE(_hubble_mac_frame_symbols);

	return length <= floor((double)((_hubble_mac_frame_symbols[idx - 1] -
			HUBBLE_MAC_HEADER_SYMBOLS_SIZE) * HUBBLE_SYMBOL_SIZE) / 8);
}

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

	return index;
}

int hubble_sat_packet_get(struct hubble_sat_packet *packet, uint64_t device_id,
			  const void *payload, size_t length)
{
	int ret;
	struct hubble_bitarray bit_array;
	uint8_t number_of_symbols;
	uint8_t number_of_padding_symbols;
	uint8_t packet_length;
	uint8_t symbol_index;
	uint8_t ecc;
	int symbols[HUBBLE_PACKET_FRAME_MAX_SIZE];
	int *rs_symbols;

	if (!_payload_length_check(length)) {
		return -EINVAL;
	}

	hubble_bitarray_init(&bit_array);

	/* Device ID */
	ret = hubble_bitarray_append(&bit_array,
                                     (uint8_t *)&device_id,
				     HUBBLE_DEVICE_ID_SIZE);
	if (ret < 0) {
		return ret;
	}

	/* Sequence number */
	/* TODO: We need to protect _sequence_number  */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&_sequence_number,
				     HUBBLE_SEQUENCE_NUMBER_SIZE);
	if (ret < 0) {
		return ret;
	}
	_sequence_number++;

	/* Authentication tag */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&(uint16_t){0},
				     HUBBLE_AUTH_TAG_SIZE);
	if (ret < 0) {
		return ret;
	}

	/* Payload */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)payload,
				     length * HUBBLE_CHAR_BITS);
	if (ret < 0) {
		return ret;
	}

	/* Alignment bit + padding [0 ... 5] */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&(uint8_t){1}, 1);
	if (ret < 0) {
		return ret;
	}
	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){0},
		HUBBLE_SYMBOL_SIZE - (bit_array.index % HUBBLE_SYMBOL_SIZE));
	if (ret < 0) {
		return ret;
	}

	/* Calculate the number of padding symbols needed to fill the packet */
	number_of_symbols = bit_array.index / HUBBLE_SYMBOL_SIZE;
	symbol_index = _mac_total_symbols_index_get(number_of_symbols);
	number_of_padding_symbols =
		_hubble_mac_frame_symbols[symbol_index] - number_of_symbols;
	if (number_of_padding_symbols > 0) {
		ret = hubble_bitarray_append(
			&bit_array, (uint8_t *)&(uint16_t){0},
			number_of_padding_symbols * HUBBLE_SYMBOL_SIZE);
		if (ret < 0) {
			return ret;
		}
		number_of_symbols++;
	}

	ret = _encode(&bit_array, symbols, HUBBLE_PACKET_MAX_SIZE);

	/* Packet length field == 11 + <5 bit value represented in header symbols> */
	packet_length = symbol_index;

	/* generate error control symbols */
	rse_gf_generate();

	ecc = _hubble_mac_error_control_symbols[symbol_index] / 2;
	rse_poly_generate(ecc);
	rs_symbols = rse_rs_encode(symbols, number_of_symbols, ecc);

	for (uint8_t mac_idx = 0, rs_idx = 0, i = 0;
	     i < _hubble_packet_total_symbols[symbol_index]; i++) {
		if ((i == 0) || (i == 9) || (i == 18)) {
			packet->data[i] = packet_length;
			continue;
		}

		if (mac_idx < _hubble_mac_frame_symbols[symbol_index]) {
			packet->data[i] = symbols[mac_idx];
			mac_idx++;
		} else {
			packet->data[i] = rs_symbols[rs_idx];
			rs_idx++;
		}
	}

	packet->length = _hubble_packet_total_symbols[symbol_index];
	return 0;
}
