/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Standard C Libraries */
#include <errno.h>

/* Hubble */
#include <hubble/port/sat_radio.h>
#include <hubble/sat/packet.h>

/* FreeRTOS / ESP-IDF */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gptimer.h"
#include "esp_phy_init.h"
#include "esp_phy_cert_test.h"

#include "sat_board.h"

#define ESP_RADIO_OFF_DELAY_US          450U
#define ESP_RADIO_ON_DELAY_US           70U
#define MAX_TX_POWER_DBM                20U
#define ESP_STEP_SCALE(_step)           ((_step) * 4)

/* The center frequency for channel 0 is 2482208625
 * -> f_base = 2482208625 - (32 * 400) = 2482195825
 * Each channel is 25.75 kHz
 * --> offset = 25.75k / 400 ~= 64 steps (round)
 * Base is set at 24822 MHz,
 * (2482195825 - 2.482e9) / 400 = 489 steps
 */
#define HUBBLE_BASE_FREQUENCY           2482U
#define HUBBLE_CHANNEL_OFFSET(_channel) (((_channel) * 64) + 489)

/**
 * This semaphore is used to protect a packet transmission and avoid
 * race conditions.
 */
static SemaphoreHandle_t _transmit_sem;

/**
 * This semaphore is used between symbol transmissions. It allows the current
 * thread to wait a symbol transmission without need to poll for a condition.
 */
static SemaphoreHandle_t _symbol_sem;

/**
 * General Purpose Timer for symbol timing.
 */
static gptimer_handle_t _timer_handle = NULL;
static gptimer_config_t _timer_config = {
	.clk_src = GPTIMER_CLK_SRC_DEFAULT, /* Select the default clock source */
	.direction = GPTIMER_COUNT_UP,      /* Counting direction is up */
	.resolution_hz = 1 * 1000 * 1000,   /* 1 Mhz -> 1 tick = 1 us */
};

/* Current sat power */
uint8_t _sat_power_dbm = MAX_TX_POWER_DBM;

/* Espressif PHY API */
extern void phy_set_step_01k(bool step_01k);
extern void phy_set_freq(uint16_t freq_mhz, int offset);
extern void phy_tx_tone(bool txtone_en, bool bt_mode, uint8_t pwr_index);

static int _esp_err_to_errno(esp_err_t status)
{
	int ret;

	switch (status) {
	case ESP_OK:
		ret = 0;
		break;
	case ESP_FAIL:
		ret = -EIO;
		break;
	case ESP_ERR_NO_MEM:
		ret = -ENOMEM;
		break;
	case ESP_ERR_INVALID_ARG:
		ret = -EINVAL;
		break;
	case ESP_ERR_INVALID_STATE:
		ret = -EPERM;
		break;
	case ESP_ERR_INVALID_SIZE:
		ret = -EINVAL;
		break;
	case ESP_ERR_NOT_FOUND:
		ret = -ENOENT;
		break;
	case ESP_ERR_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	case ESP_ERR_TIMEOUT:
		ret = -ETIMEDOUT;
		break;
	case ESP_ERR_INVALID_RESPONSE:
		ret = -EBADMSG;
		break;
	case ESP_ERR_NOT_FINISHED:
		ret = -EINPROGRESS;
		break;
	case ESP_ERR_NOT_ALLOWED:
		ret = -EACCES;
		break;
	default:
		/* Let's use EIO as a generic error */
		ret = -EIO;
		break;
	}

	return ret;
}

static bool _timer_cb(gptimer_handle_t timer,
		      const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
	BaseType_t higher_prio_task_woken = pdFALSE;

	(void)gptimer_stop(timer);
	xSemaphoreGiveFromISR(_symbol_sem, &higher_prio_task_woken);

	/*
	 * Return value: whether a high-priority task was awakened
	 * to notify the scheduler to switch tasks
	 */
	return (higher_prio_task_woken == pdTRUE);
}

static int _timer_init(void)
{
	esp_err_t ret;

	ret = gptimer_new_timer(&_timer_config, &_timer_handle);
	if (ret != ESP_OK) {
		return _esp_err_to_errno(ret);
	}

	gptimer_event_callbacks_t cb = {
		.on_alarm = _timer_cb,
	};

	ret = gptimer_register_event_callbacks(_timer_handle, &cb, NULL);
	if (ret != ESP_OK) {
		(void)gptimer_del_timer(_timer_handle);
		return _esp_err_to_errno(ret);
	}

	return 0;
}

static int _timer_start(uint32_t period_us)
{
	esp_err_t err;
	gptimer_alarm_config_t alarm_config = {
		.alarm_count = period_us,
	};

	err = gptimer_set_raw_count(_timer_handle, 0);
	if (err != ESP_OK) {
		return _esp_err_to_errno(err);
	}

	err = gptimer_set_alarm_action(_timer_handle, &alarm_config);
	if (err != ESP_OK) {
		return _esp_err_to_errno(err);
	}
	return _esp_err_to_errno(gptimer_start(_timer_handle));
}

static int _timer_enable(void)
{
	return _esp_err_to_errno(gptimer_enable(_timer_handle));
}

static int _timer_disable(void)
{
	(void)gptimer_stop(_timer_handle);
	return _esp_err_to_errno(gptimer_disable(_timer_handle));
}

static int _radio_cw_start(uint16_t step, uint32_t delay, uint32_t duration_us)
{
	int ret;

	/* Symbol on time */
	phy_set_freq(HUBBLE_BASE_FREQUENCY, step);
	phy_tx_tone(true, true, _sat_power_dbm);

	ret = _timer_start(duration_us);
	if (ret != 0) {
		phy_tx_tone(false, true, _sat_power_dbm);
		return ret;
	}

	xSemaphoreTake(_symbol_sem, portMAX_DELAY);

	/* Symbol off time */
	phy_tx_tone(false, true, _sat_power_dbm);

	ret = _timer_start(delay);
	if (ret != 0) {
		return ret;
	}

	xSemaphoreTake(_symbol_sem, portMAX_DELAY);
	return 0;
}

int hubble_sat_board_init(void)
{
	int ret;

	ret = _timer_init();
	if (ret != 0) {
		return ret;
	}

	_symbol_sem = xSemaphoreCreateBinary();
	if (_symbol_sem == NULL) {
		(void)gptimer_del_timer(_timer_handle);
		return -ENOMEM;
	}

	_transmit_sem = xSemaphoreCreateBinary();
	if (_transmit_sem == NULL) {
		(void)gptimer_del_timer(_timer_handle);
		vSemaphoreDelete(_symbol_sem);
		_symbol_sem = NULL;
		return -ENOMEM;
	}

	/* Make count to 1 initially */
	if (xSemaphoreGive(_transmit_sem) != pdTRUE) {
		(void)gptimer_del_timer(_timer_handle);
		vSemaphoreDelete(_transmit_sem);
		vSemaphoreDelete(_symbol_sem);

		_transmit_sem = NULL;
		_symbol_sem = NULL;

		return -EAGAIN;
	}

	esp_phy_rftest_init();
	return 0;
}

int hubble_sat_board_enable(void)
{
	int ret = _timer_enable();
	if (ret != 0) {
		return ret;
	}

	esp_phy_rftest_config(1);
	phy_set_step_01k(true);

	return 0;
}

int hubble_sat_board_disable(void)
{
	esp_phy_rftest_config(0);
	phy_set_step_01k(false);
	return _timer_disable();
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	int ret = 0;
	int8_t frame = -1;

	xSemaphoreTake(_transmit_sem, portMAX_DELAY);
	xSemaphoreTake(_symbol_sem, 0); /* Reset semaphore */

	for (size_t i = 0; i < packet->total_number_of_symbols; i++) {
		uint16_t step;
		uint8_t data_pos = i % HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE;

		if (data_pos == 0) {
			frame++;
		}

		step = ESP_STEP_SCALE(
			packet->frame[frame].data[data_pos] +
			HUBBLE_CHANNEL_OFFSET(packet->frame[frame].channel));

		/*
		 * Because there's a certain delay for the radio to ramp up,
		 * subtract the compensation to the off time delay
		 */
		ret = _radio_cw_start(
			step, HUBBLE_WAIT_SYMBOL_OFF_US - ESP_RADIO_OFF_DELAY_US,
			HUBBLE_WAIT_SYMBOL_US - ESP_RADIO_ON_DELAY_US);

		if (ret != 0) {
			break;
		}
	}

	xSemaphoreGive(_transmit_sem);
	return ret;
}

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE

int hubble_sat_board_power_set(int8_t power)
{
	if (power < 0 || power > MAX_TX_POWER_DBM) {
		return -EINVAL;
	}

	_sat_power_dbm = power;
	return 0;
}

int hubble_sat_board_cw_start(uint8_t channel)
{
	uint16_t step = ESP_STEP_SCALE(32 + HUBBLE_CHANNEL_OFFSET(channel));
	phy_set_freq(HUBBLE_BASE_FREQUENCY, step);
	phy_tx_tone(true, true, _sat_power_dbm);
	return 0;
}

int hubble_sat_board_cw_stop(void)
{
	phy_tx_tone(false, true, _sat_power_dbm);
	return 0;
}

#endif /* CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE */
