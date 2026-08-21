/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sat_soc.h>

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <hubble/port/sat_radio.h>

#include <hal/nrf_radio.h>

#include <errno.h>
#include <stdint.h>

#ifdef CONFIG_SOC_COMPATIBLE_NRF5340_CPUNET

/* Needs this for increasing the SoC power above 0 */
#include <hal/nrf_vreqctrl.h>

#ifndef RADIO_TXPOWER_TXPOWER_Pos3dBm
#define RADIO_TXPOWER_TXPOWER_Pos3dBm (0x03UL)
#define NRF_RADIO_TXPOWER_POS3DBM     RADIO_TXPOWER_TXPOWER_Pos3dBm
#endif /* RADIO_TXPOWER_TXPOWER_Pos3dBm */

#ifndef RADIO_TXPOWER_TXPOWER_Pos2dBm
#define RADIO_TXPOWER_TXPOWER_Pos2dBm (0x02UL)
#define NRF_RADIO_TXPOWER_POS2DBM     RADIO_TXPOWER_TXPOWER_Pos2dBm
#endif /* RADIO_TXPOWER_TXPOWER_Pos2dBm */

#ifndef RADIO_TXPOWER_TXPOWER_Pos1dBm
#define RADIO_TXPOWER_TXPOWER_Pos1dBm (0x01UL)
#define NRF_RADIO_TXPOWER_POS1DBM     RADIO_TXPOWER_TXPOWER_Pos1dBm
#endif /* RADIO_TXPOWER_TXPOWER_Pos1dBm */

#endif /* CONFIG_SOC_COMPATIBLE_NRF5340_CPUNET */

extern nrf_radio_txpower_t _sat_power;

/**
 * Signature for APIs provided by the binary library.
 */
int hubble_nrf_lib_frequency_set(uint8_t channel, uint8_t step);

int hubble_sat_soc_power_set(int8_t power)
{
	switch (power) {
#ifdef RADIO_TXPOWER_TXPOWER_Pos8dBm
	case 8:
		_sat_power = NRF_RADIO_TXPOWER_POS8DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos7dBm
	case 7:
		_sat_power = NRF_RADIO_TXPOWER_POS7DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos6dBm
	case 6:
		_sat_power = NRF_RADIO_TXPOWER_POS6DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos5dBm
	case 5:
		_sat_power = NRF_RADIO_TXPOWER_POS5DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos4dBm
	case 4:
		_sat_power = NRF_RADIO_TXPOWER_POS4DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos3dBm
	case 3:
		_sat_power = NRF_RADIO_TXPOWER_POS3DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos2dBm
	case 2:
		_sat_power = NRF_RADIO_TXPOWER_POS2DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Pos1dBm
	case 1:
		_sat_power = NRF_RADIO_TXPOWER_POS1DBM;
		break;
#endif
	case 0:
		_sat_power = NRF_RADIO_TXPOWER_0DBM;
		break;
#ifdef RADIO_TXPOWER_TXPOWER_Neg1dBm
	case -1:
		_sat_power = NRF_RADIO_TXPOWER_NEG1DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg2dBm
	case -2:
		_sat_power = NRF_RADIO_TXPOWER_NEG2DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg3dBm
	case -3:
		_sat_power = NRF_RADIO_TXPOWER_NEG3DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg4dBm
	case -4:
		_sat_power = NRF_RADIO_TXPOWER_NEG4DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg5dBm
	case -5:
		_sat_power = NRF_RADIO_TXPOWER_NEG5DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg6dBm
	case -6:
		_sat_power = NRF_RADIO_TXPOWER_NEG6DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg7dBm
	case -7:
		_sat_power = NRF_RADIO_TXPOWER_NEG7DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg8dBm
	case -8:
		_sat_power = NRF_RADIO_TXPOWER_NEG8DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg9dBm
	case -9:
		_sat_power = NRF_RADIO_TXPOWER_NEG9DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg10dBm
	case -10:
		_sat_power = NRF_RADIO_TXPOWER_NEG10DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg12dBm
	case -12:
		_sat_power = NRF_RADIO_TXPOWER_NEG12DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg14dBm
	case -14:
		_sat_power = NRF_RADIO_TXPOWER_NEG14DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg16dBm
	case -16:
		_sat_power = NRF_RADIO_TXPOWER_NEG16DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg20dBm
	case -20:
		_sat_power = NRF_RADIO_TXPOWER_NEG20DBM;
		break;
#endif
#ifdef RADIO_TXPOWER_TXPOWER_Neg40dBm
	case -40:
		_sat_power = NRF_RADIO_TXPOWER_NEG40DBM;
		break;
#endif
	default:
		return -EINVAL;
	}

#ifdef CONFIG_SOC_COMPATIBLE_NRF5340_CPUNET
	if (_sat_power > 0) {
		/* High voltage increase the output power by 3 dB */
		_sat_power -= 3;
		nrf_vreqctrl_radio_high_voltage_set(NRF_VREQCTRL, true);
	} else {
		nrf_vreqctrl_radio_high_voltage_set(NRF_VREQCTRL, false);
	}
#endif /* CONFIG_SOC_COMPATIBLE_NRF5340_CPUNET */

	nrf_radio_txpower_set(NRF_RADIO, _sat_power);
	return 0;
}

int hubble_sat_soc_cw_start(uint8_t channel)
{
	int ret = hubble_nrf_lib_frequency_set(channel, 32);

	if (ret != 0) {
		return ret;
	}

	nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_TXEN);
#if defined(RADIO_INTENSET_TXREADY_Msk) || defined(RADIO_INTENSET00_TXREADY_Msk)
	while (!nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_TXREADY)) {
		/* Do nothing */
	}
#endif

	return 0;
}

int hubble_sat_soc_cw_stop(void)
{
	nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_DISABLE);

	while (!nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_DISABLED)) {
		/* Do nothing */
	}
	nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_DISABLED);

	return 0;
}
