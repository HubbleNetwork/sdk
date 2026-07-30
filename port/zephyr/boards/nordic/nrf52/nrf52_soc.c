/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sat_soc.h>

#include <nrfx_timer.h>
#include <hal/nrf_radio.h>
#include <hal/nrf_ppi.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

#include <hubble/port/sat_radio.h>
#include <hubble/sat/packet.h>

#include <stdint.h>
#include <errno.h>

#define RADIO_NODE         DT_NODELABEL(radio)

/* From NRF52840_PS_v1.2 6.20.15.8 Time between TXEN -> READ is 140us */
#define WAIT_SYMBOL_OFF_US (HUBBLE_WAIT_SYMBOL_OFF_US - 140)
#define WAIT_SYMBOL_US     (HUBBLE_WAIT_SYMBOL_US + 140)

static uint32_t _radio_shorts;

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

static void _ppi_disable(void)
{
	nrf_ppi_channel_disable(NRF_PPI, NRF_PPI_CHANNEL22);
	nrf_ppi_channel_disable(NRF_PPI, NRF_PPI_CHANNEL20);
}

static void _ppi_enable(void)
{
	nrf_ppi_channel_enable(NRF_PPI, NRF_PPI_CHANNEL22);
	nrf_ppi_channel_enable(NRF_PPI, NRF_PPI_CHANNEL20);
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

int hubble_sat_soc_enable(void)
{
	int ret;
	const struct device *const clock0 = DEVICE_DT_GET_ONE(nordic_nrf_clock);

	if (!device_is_ready(clock0)) {
		return -ENODEV;
	}

	ret = clock_control_on(clock0, CLOCK_CONTROL_NRF_SUBSYS_HF);
	if ((ret < 0) && (ret != -EALREADY)) {
		return ret;
	}

	(void)hubble_nrf_lib_enable();

	_radio_shorts = nrf_radio_shorts_get(NRF_RADIO);
	nrf_radio_shorts_disable(NRF_RADIO, ~0);
	nrf_radio_int_enable(NRF_RADIO, NRF_RADIO_INT_DISABLED_MASK);

	nrf_radio_mode_set(NRF_RADIO, NRF_RADIO_MODE_NRF_1MBIT);

	_timer_setup();

#ifdef CONFIG_DYNAMIC_INTERRUPTS
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
	irq_enable(RADIO_IRQn);

	_normal_power = nrf_radio_txpower_get(NRF_RADIO);
	nrf_radio_txpower_set(NRF_RADIO, _sat_power);

	return 0;
}

int hubble_sat_soc_disable(void)
{
	(void)hubble_nrf_lib_disable();

	irq_disable(DT_IRQN(RADIO_NODE));

#if defined(CONFIG_DYNAMIC_INTERRUPTS) && defined(CONFIG_SHARED_INTERRUPTS)
	irq_disconnect_dynamic(DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority),
			       _radio_isr, NULL, 0);
#endif /* CONFIG_DYNAMIC_INTERRUPTS  && CONFIG_SHARED_INTERRUPTS */

	nrf_radio_shorts_set(NRF_RADIO, _radio_shorts);
	nrf_radio_txpower_set(NRF_RADIO, _normal_power);

	return 0;
}

int hubble_sat_soc_packet_send(const struct hubble_sat_packet_frames *packet)
{
	int8_t frame = -1;

	k_sem_take(&_transmit_sem, K_FOREVER);

	k_sem_reset(&_symbol_sem);

	_ppi_enable();
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

	_ppi_disable();
	_timer_disable();

	k_sem_give(&_transmit_sem);

	return 0;
}
