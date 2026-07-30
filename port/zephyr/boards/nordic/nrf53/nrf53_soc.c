/*
 * Copyright (c) 2025 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sat_soc.h>

#include <stdint.h>

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

#include <hubble/port/sat_radio.h>
#include <hubble/sat/packet.h>

#include <nrfx_timer.h>
#include <hal/nrf_power.h>
#include <hal/nrf_egu.h>
#include <hal/nrf_radio.h>
#include <hal/nrf_dppi.h>
#include <hal/nrf_timer.h>

#include <errno.h>

#define RADIO_NODE                 DT_NODELABEL(radio)

/**
 * Time between TXEN task and READY event with fast ramp-up enabled is
 * 40us. nRF5340_PS_v1.6 - 7.26.16.8
 */
#define WAIT_SYMBOL_OFF_US         (HUBBLE_WAIT_SYMBOL_OFF_US - 40)
#define WAIT_SYMBOL_US             (HUBBLE_WAIT_SYMBOL_US + 40)

#define RADIO_ENABLE_TX_ON_CC0_PPI 9U
#define RADIO_DISABLE_ON_CC1_PPI   12U

static const struct device *const clock0 = DEVICE_DT_GET_ONE(nordic_nrf_clock);

static nrfx_timer_t _timer0 = NRFX_TIMER_INSTANCE(NRF_TIMER_INST_GET(0));

/* Keep track of power before and in sat tx mode */
static nrf_radio_txpower_t _normal_power = RADIO_TXPOWER_TXPOWER_0dBm;
nrf_radio_txpower_t _sat_power = RADIO_TXPOWER_TXPOWER_0dBm;

/**
 * Signature for APIs provided by the binary library.
 */
int hubble_nrf_lib_frequency_set(uint8_t channel, uint8_t step);
int hubble_nrf_lib_enable(void);
int hubble_nrf_lib_disable(void);

/**
 * This semaphore is used to protect a packet transmission and avoid
 * race conditions.
 */
K_SEM_DEFINE(_transmit_sem, 1, 1);

/**
 * This semaphore is used between symbol transmissions. It allows the current
 * thread to wait for a symbol transmission without needing to poll.
 */
K_SEM_DEFINE(_symbol_sem, 0, 1);


static void _dppi_setup(void)
{
	/* TIMER COMPARE[0] -> RADIO TXEN */
	nrf_timer_publish_set(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE0,
			      RADIO_ENABLE_TX_ON_CC0_PPI);
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_TXEN,
				RADIO_ENABLE_TX_ON_CC0_PPI);

	/* Address nRF5340 Engineering A Errata 16 */
	nrf_radio_subscribe_clear(NRF_RADIO, NRF_RADIO_TASK_RXEN);

	/* TIMER COMPARE[1] -> RADIO DISABLE */
	nrf_timer_publish_set(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE1,
			      RADIO_DISABLE_ON_CC1_PPI);
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_DISABLE,
				RADIO_DISABLE_ON_CC1_PPI);
}

static void _dppi_enable(void)
{
	nrf_dppi_channels_enable(NRF_DPPIC,
				 BIT(RADIO_ENABLE_TX_ON_CC0_PPI) |
					 BIT(RADIO_DISABLE_ON_CC1_PPI));
}

static void _dppi_disable(void)
{
	nrf_dppi_channels_disable(NRF_DPPIC,
				  BIT(RADIO_ENABLE_TX_ON_CC0_PPI) |
					  BIT(RADIO_DISABLE_ON_CC1_PPI));
}

static void _dppi_clear(void)
{
	nrf_timer_publish_clear(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE0);
	nrf_timer_publish_clear(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE1);
	nrf_radio_subscribe_clear(NRF_RADIO, NRF_RADIO_TASK_TXEN);
	nrf_radio_subscribe_clear(NRF_RADIO, NRF_RADIO_TASK_DISABLE);
}

static void _timer_enable(uint32_t cc0, uint32_t cc1)
{
	nrf_timer_shorts_enable(_timer0.p_reg,
				NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK |
					NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);

	nrfx_timer_extended_compare(&_timer0, NRF_TIMER_CC_CHANNEL1,
				    nrfx_timer_us_to_ticks(&_timer0, cc1),
				    NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK, false);

	nrfx_timer_extended_compare(&_timer0, NRF_TIMER_CC_CHANNEL0,
				    nrfx_timer_us_to_ticks(&_timer0, cc0), 0,
				    false);

	nrfx_timer_clear(&_timer0);
	nrfx_timer_enable(&_timer0);
}

static void _timer_disable(void)
{
	nrfx_timer_disable(&_timer0);
}

static void _timer_setup(void)
{
	nrfx_timer_config_t timer_cfg = {
		.frequency = NRF_TIMER_FREQ_1MHz,
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
	};

	nrfx_timer_init(&_timer0, &timer_cfg, NULL);
}

static void _radio_isr(const void *arg)
{
	if (nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_DISABLED)) {
		nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_DISABLED);
		k_sem_give(&_symbol_sem);
	}
}

int hubble_sat_soc_disable(void)
{
	nrf_radio_shorts_set(NRF_RADIO, 0);
	nrf_radio_int_disable(NRF_RADIO, ~0);

	_dppi_clear();

	irq_disable(DT_IRQN(RADIO_NODE));

#if defined(CONFIG_DYNAMIC_INTERRUPTS) && defined(CONFIG_SHARED_INTERRUPTS)
	irq_disconnect_dynamic(DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority),
			       _radio_isr, NULL, 0);
#endif /* CONFIG_DYNAMIC_INTERRUPTS  && CONFIG_SHARED_INTERRUPTS */

	(void)hubble_nrf_lib_disable();

	nrf_radio_txpower_set(NRF_RADIO, _normal_power);

	return 0;
}

int hubble_sat_soc_enable(void)
{
	int ret;

	if (!device_is_ready(clock0)) {
		return -ENODEV;
	}

	ret = clock_control_on(clock0, CLOCK_CONTROL_NRF_SUBSYS_HF);
	if ((ret < 0) && (ret != -EALREADY)) {
		return ret;
	}

	nrf_radio_fast_ramp_up_enable_set(NRF_RADIO, true);
	nrf_radio_mode_set(NRF_RADIO, NRF_RADIO_MODE_BLE_1MBIT);
	nrf_radio_shorts_enable(NRF_RADIO, NRF_RADIO_SHORT_READY_START_MASK);
	nrf_radio_int_enable(NRF_RADIO, NRF_RADIO_INT_DISABLED_MASK);

	(void)hubble_nrf_lib_enable();

	_timer_setup();
	_dppi_setup();

#if defined(CONFIG_DYNAMIC_INTERRUPTS) && defined(CONFIG_SHARED_INTERRUPTS)
#ifdef CONFIG_DYNAMIC_DIRECT_INTERRUPTS
	ARM_IRQ_DIRECT_DYNAMIC_CONNECT(
		DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority), 0, reschedule);
#endif /* CONFIG_DYNAMIC_DIRECT_INTERRUPTS */
	irq_connect_dynamic(DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority),
			    _radio_isr, NULL, 0);
#else  /* !CONFIG_DYNAMIC_INTERRUPTS */
	IRQ_CONNECT(DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority),
		    _radio_isr, NULL, 0);
#endif /* CONFIG_DYNAMIC_INTERRUPTS */

	irq_enable(DT_IRQN(RADIO_NODE));

	_normal_power = nrf_radio_txpower_get(NRF_RADIO);
	nrf_radio_txpower_set(NRF_RADIO, _sat_power);

	return 0;
}

int hubble_sat_soc_packet_send(const struct hubble_sat_packet_frames *packet)
{
	int8_t frame = -1;

	k_sem_take(&_transmit_sem, K_FOREVER);
	k_sem_reset(&_symbol_sem);

	_dppi_enable();
	_timer_enable(WAIT_SYMBOL_OFF_US, WAIT_SYMBOL_OFF_US + WAIT_SYMBOL_US);

	for (size_t i = 0; i < packet->total_number_of_symbols; i++) {
		uint8_t data_pos = i % HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE;

		if (data_pos == 0) {
			frame++;
		}

		hubble_nrf_lib_frequency_set(packet->frame[frame].channel,
					     packet->frame[frame].data[data_pos]);
		k_sem_take(&_symbol_sem, K_FOREVER);
	}

	_dppi_disable();
	_timer_disable();

	k_sem_give(&_transmit_sem);

	return 0;
}
