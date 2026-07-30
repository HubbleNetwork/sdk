/*
 * Copyright (c) 2026 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>
#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE
#include <hubble/sat/dtm.h>
#endif

#include "reed_solomon_encoder.h"
#include "hubble_priv.h"
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

/* Two bits used -> 2**2 */
#define HUBBLE_HOPPING_SEQUENCE_INFO_NUM     4U

static const int8_t _preamble[] = HUBBLE_SAT_PREAMBLE_SEQUENCE;

/* This is pseudorandom pre-computed list of channel hopping. */
static uint8_t _channel_hops[HUBBLE_HOPPING_SEQUENCE_INFO_NUM][HUBBLE_SAT_NUM_CHANNELS] = {
	{3, 14, 5, 6, 9, 2, 12, 8, 15, 4, 11, 13, 17, 10, 1, 7, 0, 18, 16},
	{10, 3, 15, 5, 0, 17, 13, 6, 11, 4, 8, 18, 9, 14, 1, 12, 7, 16, 2},
	{14, 5, 11, 3, 8, 2, 18, 4, 10, 13, 9, 1, 16, 17, 0, 6, 15, 12, 7},
	{7, 0, 11, 18, 4, 2, 13, 5, 10, 17, 3, 9, 16, 14, 8, 12, 1, 6, 15},
};

static uint8_t _hopping_sequence;
static uint8_t _last_used_channel;

static uint8_t _channel_idx_find(uint8_t hopping_sequence, uint8_t initial_channel)
{
	for (uint8_t idx = 0; idx < HUBBLE_SAT_NUM_CHANNELS; idx++) {
		if (_channel_hops[hopping_sequence][idx] == initial_channel) {
			return idx;
		}
	}

	/* This condition should never happen */
	return 0;
}

static uint8_t _channel_get(void)
{
	uint8_t idx;

	idx = (_channel_idx_find(_hopping_sequence, _last_used_channel) + 1) %
	      HUBBLE_SAT_NUM_CHANNELS;
	_last_used_channel = _channel_hops[_hopping_sequence][idx];

	return _last_used_channel;
}

void hubble_internal_channel_hopping_sequence_set(void)
{
	if (hubble_rand_get(&_hopping_sequence, sizeof(_hopping_sequence))) {
		_hopping_sequence = 0;
		HUBBLE_LOG_WARNING("Could not pick a random hopping sequence");
	} else {
		_hopping_sequence =
			_hopping_sequence % HUBBLE_HOPPING_SEQUENCE_INFO_NUM;
	}

	_last_used_channel =
		_channel_hops[_hopping_sequence][HUBBLE_SAT_NUM_CHANNELS - 1];
}

static int _encode(const struct hubble_bitarray *bit_array, int *symbols,
		   size_t symbols_size)
{
	uint8_t symbol = 0U;
	int symbol_bit_index = 0;
	int index = 0;

	/**
	 * Account for the trailing padding symbol written when the bit count is
	 * not a multiple of HUBBLE_SYMBOL_SIZE.
	 */
	if (((bit_array->index + HUBBLE_SYMBOL_SIZE - 1) / HUBBLE_SYMBOL_SIZE) >
	    symbols_size) {
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

static int8_t _packet_payload_ecc_get(size_t len)
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
		ecc = -EINVAL;
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
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _packet_phy_size_get(size_t pdu_len)
{
	int ret;

	switch (pdu_len) {
	case 23:
		ret = 0b00;
		break;
	case 30:
		ret = 0b01;
		break;
	case 39:
		ret = 0b10;
		break;
	case 46:
		ret = 0b11;
		break;
#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE
	/* DTM single-frame mode uses a sentinel length that has no PHY size
	 * mapping; the payload is not transmitted, so fall back to 0.
	 */
	case HUBBLE_SAT_DTM_PACKET_ONE_FRAME_ONLY_LEN:
		ret = 0b00;
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _whitening(uint8_t seed, uint8_t *symbols, size_t len)
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

int hubble_sat_packet_get(struct hubble_sat_packet *packet, const void *payload,
			  size_t length)
{
	int ret;
	struct hubble_bitarray bit_array;
	int symbols[HUBBLE_PACKET_MAX_SIZE] = {0};
	int *rs_symbols;
	uint8_t payload_len, ecc;
	uint8_t auth_tag[HUBBLE_AUTH_TAG_SIZE / HUBBLE_BITS_PER_BYTE];
	uint8_t out[HUBBLE_PAYLOAD_MAX_SIZE] = {0};
	uint16_t seq_no;
	uint32_t time_counter;
	uint32_t eid;

	if ((packet == NULL) || ((payload == NULL) && (length > 0))) {
		return -EINVAL;
	}

	if (hubble_internal_key_get() == NULL) {
		HUBBLE_LOG_WARNING("Key not set");
		return -EINVAL;
	}

	/* Use the configured counter source (Unix time or device uptime) so
	 * satellite packets honor CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME,
	 * matching the BLE advertisement path.
	 */
	ret = hubble_counter_get(&time_counter);
	if (ret != 0) {
		return ret;
	}

	ret = hubble_internal_sequence_acquire(time_counter, &seq_no);
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Re-using same nonce is insecure !");
		return ret;
	}

#define _CHECK_RET(_ret)                                                       \
	if (_ret < 0) {                                                        \
		return _ret;                                                   \
	}

	ret = _packet_payload_size_get(length);
	_CHECK_RET(ret);

	payload_len = ret;
	/* Packet payload now. */
	ret = hubble_internal_device_id_get((uint8_t *)&eid, sizeof(eid),
					    time_counter);
	_CHECK_RET(ret);

	ret = hubble_internal_data_encrypt(time_counter, seq_no, payload, length,
					   out, auth_tag, sizeof(auth_tag));
	_CHECK_RET(ret);

	hubble_bitarray_init(&bit_array);

	/* Payload version */
	ret = hubble_bitarray_append(
		&bit_array,
		(uint8_t *)&(uint8_t){HUBBLE_PAYLOAD_PROTOCOL_VERSION},
		HUBBLE_PAYLOAD_PROTOCOL_VERSION_SIZE);
	_CHECK_RET(ret);

	/* Sequence number */
	ret = hubble_bitarray_append(&bit_array, (uint8_t *)&seq_no,
				     HUBBLE_SEQUENCE_NUMBER_SIZE);
	_CHECK_RET(ret);

	/* Device ID */
	ret = hubble_bitarray_append_big(&bit_array, (uint8_t *)&eid,
					 HUBBLE_DEVICE_ID_SIZE);
	_CHECK_RET(ret);

	/* Authentication tag */
	ret = hubble_bitarray_append_big(&bit_array, auth_tag,
					 HUBBLE_AUTH_TAG_SIZE);
	_CHECK_RET(ret);

	/* Payload */
	ret = hubble_bitarray_append_big(&bit_array, (uint8_t *)out,
					 length * HUBBLE_BITS_PER_BYTE);
	_CHECK_RET(ret);

	/* This returns the number of symbols */
	ret = _encode(&bit_array, symbols, HUBBLE_PACKET_MAX_SIZE);
	_CHECK_RET(ret);

	/* generate error control symbols */
	rse_gf_generate();
	ret = _packet_payload_ecc_get(length);
	_CHECK_RET(ret);

	ecc = ret;

	rse_poly_generate(ecc / 2);
	rs_symbols = rse_rs_encode(symbols, ret, ecc / 2);

	/* We need to append rs_symbols to symbols before whitening them
	 * due lfsr7 state.
	 */
	memcpy(&symbols[ret], rs_symbols, ecc * sizeof(int));
	packet->length = payload_len + ecc;

	for (size_t i = 0; i < packet->length; i++) {
		packet->data[i] = symbols[i];
	}

#undef _CHECK_RET

	return 0;
}

int hubble_sat_packet_frames_get(const struct hubble_sat_packet *packet,
				 struct hubble_sat_packet_frames *frames)
{
	int ret;
	struct hubble_bitarray bit_array;
	int symbols[HUBBLE_PACKET_MAX_SIZE] = {0};
	int *rs_symbols;
	uint8_t phy_len, channel, hop_sequence, frames_size = 0, frame_pos = 0;

#define _CHECK_RET(_ret)                                                       \
	if (_ret < 0) {                                                        \
		return _ret;                                                   \
	}

	if ((packet == NULL) || (frames == NULL)) {
		return -EINVAL;
	}

	/* Validate the packet length (which maps to the PHY payload size field)
	 * before mutating any channel-hopping state or touching the output.
	 */
	ret = _packet_phy_size_get(packet->length);
	_CHECK_RET(ret);

	phy_len = (uint8_t)ret;

	hop_sequence = _hopping_sequence;
	channel = _channel_get();

	frames->frame[0].channel = channel;

	hubble_bitarray_init(&bit_array);
	ret = hubble_bitarray_append(
		&bit_array, (uint8_t *)&(uint8_t){HUBBLE_PHY_PROTOCOL_VERSION},
		HUBBLE_PHY_PROTOCOL_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(&bit_array, &phy_len,
				     HUBBLE_PHY_PAYLOAD_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(&bit_array, &hop_sequence,
				     HUBBLE_PHY_HOP_INFO_SIZE);
	_CHECK_RET(ret);

	ret = hubble_bitarray_append(&bit_array, &channel,
				     HUBBLE_PHY_CHANNEL_SIZE);
	_CHECK_RET(ret);

	ret = _encode(&bit_array, symbols, HUBBLE_PHY_SYMBOLS_SIZE);
	_CHECK_RET(ret);

	/* Start to populate the frames so we can re-use the variable
	 * symbols.
	 */

	/* First lets fill the preamble */
	for (uint8_t i = 0; i < sizeof(_preamble); i++) {
		frames->frame[0].data[i] = _preamble[i];
	}
	frame_pos = sizeof(_preamble);

	for (uint8_t i = 0; i < HUBBLE_PHY_SYMBOLS_SIZE; i++) {
		frames->frame[0].data[frame_pos + i] = symbols[i];
	}
	frame_pos += HUBBLE_PHY_SYMBOLS_SIZE;

	rse_gf_generate();
	rse_poly_generate(HUBBLE_PHY_ECC_SYMBOLS_SIZE / 2);
	rs_symbols = rse_rs_encode(symbols, HUBBLE_PHY_SYMBOLS_SIZE,
				   HUBBLE_PHY_ECC_SYMBOLS_SIZE / 2);

	for (uint8_t i = 0; i < HUBBLE_PHY_ECC_SYMBOLS_SIZE; i++) {
		frames->frame[0].data[frame_pos + i] = rs_symbols[i];
	}
	frame_pos += HUBBLE_PHY_ECC_SYMBOLS_SIZE;

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE
	/* Payload -1 mode will cause an early return */
	if (packet->length == HUBBLE_SAT_DTM_PACKET_ONE_FRAME_ONLY_LEN) {
		frames->total_number_of_symbols =
			HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE;
		return 0;
	}

#endif /* CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE */

	/* From now on, we will need to check which frame to add data since that
	 * depends on user controlled data.
	 */

	/* data whitening symbols before add them to the packet */
	memcpy((uint8_t *)symbols, packet->data, packet->length);
	ret = _whitening(channel, (uint8_t *)symbols, packet->length);
	_CHECK_RET(ret);

	for (size_t i = 0; i < packet->length; i++) {
		if ((frame_pos % HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE) == 0) {
			frame_pos = 0;
			frames_size++;
			frames->frame[frames_size].channel = _channel_get();
		}

		frames->frame[frames_size].data[frame_pos] =
			((uint8_t *)symbols)[i];
		frame_pos++;
	}
	frames->total_number_of_symbols =
		frame_pos + (frames_size * HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE);


#undef _CHECK_RET

	return 0;
}
