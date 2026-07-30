/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sat_soc.h>

#include <zephyr/kernel.h>
#include <zephyr/arch/arm/irq.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/types.h>
#include <zephyr/version.h>
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RADIO_NODE                 DT_NODELABEL(radio)

/* From nRF54L15 PS: Time between TXEN -> READY is ~40us with fast ramp-up */
#define WAIT_SYMBOL_OFF_US         (HUBBLE_WAIT_SYMBOL_OFF_US - 40)
#define WAIT_SYMBOL_US             (HUBBLE_WAIT_SYMBOL_US + 40)

#define NRF_DPPIC                  NRF_DPPIC10
#define RADIO_ENABLE_TX_ON_CC0_PPI 9U
#define RADIO_DISABLE_ON_CC1_PPI   12U

#if ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(4, 4, 0)
static nrfx_timer_t _timer0 = NRFX_TIMER_INSTANCE(NRF_TIMER_INST_GET(10));
#else
static nrfx_timer_t _timer0 = NRFX_TIMER_INSTANCE(10);
#endif

/**
 * Signature for APIs provided by the binary library.
 */
int hubble_nrf_lib_frequency_set(uint8_t channel, uint8_t step);
int hubble_nrf_lib_enable(void);
int hubble_nrf_lib_disable(void);

static const struct device *const clock0 = DEVICE_DT_GET_ONE(nordic_nrf_clock);

/* Keep track of power before and in sat tx mode */
static nrf_radio_txpower_t _normal_power = RADIO_TXPOWER_TXPOWER_0dBm;
nrf_radio_txpower_t _sat_power = RADIO_TXPOWER_TXPOWER_0dBm;

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

/**
 * Wire EVENT_TIMER EVENTS_COMPARE[0] event to RADIO TASKS_TXEN task and
 * EVENT_TIMER EVENTS_COMPARE[1] event to RADIO TASKS_DISABLE.
 */
static void _dppi_setup(void)
{
	/* TIMER COMPARE[0] -> RADIO TXEN */
	nrf_timer_publish_set(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE0,
			      RADIO_ENABLE_TX_ON_CC0_PPI);
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_TXEN,
				RADIO_ENABLE_TX_ON_CC0_PPI);

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

/**
 * Tear down any radio/timer wiring left behind by the BLE controller.
 *
 * The BLE controller drives the radio entirely through DPPI: it leaves the
 * RADIO connected to DPPIC channels via its SUBSCRIBE and PUBLISH registers,
 * keeps those DPPIC channels enabled, may leave the radio in a non-DISABLED
 * state, and leaves stale shorts/config behind.
 */
static void _radio_reset(void)
{
	const nrf_radio_task_t tasks[] = {
		NRF_RADIO_TASK_TXEN,    NRF_RADIO_TASK_RXEN,
		NRF_RADIO_TASK_START,   NRF_RADIO_TASK_STOP,
		NRF_RADIO_TASK_DISABLE,
	};
	const nrf_radio_event_t events[] = {
		NRF_RADIO_EVENT_READY,    NRF_RADIO_EVENT_ADDRESS,
		NRF_RADIO_EVENT_PAYLOAD,  NRF_RADIO_EVENT_END,
		NRF_RADIO_EVENT_DISABLED, NRF_RADIO_EVENT_TXREADY,
	};

	/* Stop interrupts and automatic state transitions first. */
	nrf_radio_int_disable(NRF_RADIO, ~0U);
	nrf_radio_shorts_set(NRF_RADIO, 0);

	/* Force the radio to DISABLED (required before SOFTRESET and before we
	 * reconfigure it).
	 */
	if (nrf_radio_state_get(NRF_RADIO) != NRF_RADIO_STATE_DISABLED) {
		nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_DISABLED);
		nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_DISABLE);
		while (!nrf_radio_event_check(NRF_RADIO,
					      NRF_RADIO_EVENT_DISABLED)) {
		}
		nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_DISABLED);
	}

	/* Detach the radio from every DPPI channel it might still be wired to. */
	for (size_t i = 0; i < ARRAY_SIZE(tasks); i++) {
		nrf_radio_subscribe_clear(NRF_RADIO, tasks[i]);
	}
	for (size_t i = 0; i < ARRAY_SIZE(events); i++) {
		nrf_radio_publish_clear(NRF_RADIO, events[i]);
	}

#if defined(RADIO_TASKS_SOFTRESET_TASKS_SOFTRESET_Msk)
	/* Clear leftover public config (PCNF/MODECNF/frequency/...). */
	nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_SOFTRESET);
#endif

	/* Stop and detach our timer, dropping any compare events the previous
	 * owner published onto DPPI.
	 */
	nrf_timer_task_trigger(_timer0.p_reg, NRF_TIMER_TASK_STOP);
	nrf_timer_int_disable(_timer0.p_reg, ~0U);
	nrf_timer_shorts_set(_timer0.p_reg, 0);
	nrf_timer_publish_clear(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE0);
	nrf_timer_publish_clear(_timer0.p_reg, NRF_TIMER_EVENT_COMPARE1);
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

#ifdef CONFIG_DYNAMIC_INTERRUPTS
	irq_disconnect_dynamic(DT_IRQN(RADIO_NODE), DT_IRQ(RADIO_NODE, priority),
			       _radio_isr, NULL, 0);
#endif /* CONFIG_DYNAMIC_INTERRUPTS */

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

	/* Wipe out any radio/timer/DPPI state left behind by the BLE controller */
	_radio_reset();

	(void)hubble_nrf_lib_enable();

	nrf_radio_fast_ramp_up_enable_set(NRF_RADIO, true);
	nrf_radio_mode_set(NRF_RADIO, NRF_RADIO_MODE_BLE_1MBIT);
	nrf_radio_shorts_enable(NRF_RADIO, NRF_RADIO_SHORT_READY_START_MASK);
	nrf_radio_int_enable(NRF_RADIO, NRF_RADIO_INT_DISABLED_MASK);

	_timer_setup();
	_dppi_setup();

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
