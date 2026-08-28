/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ESP-IDF components */
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"

#include <hubble/hubble.h>
#include <hubble/sat.h>
#include <hubble/sat/dtm.h>
#include <hubble/sat/packet.h>

static const char *APP_TAG = "main";

#define TX_STACK_SIZE          2048
#define HUBBLE_NUM_CHANNELS    (19)
#define HUBBLE_DEFAULT_CHANNEL (0)

/* Dummy key and time because we don't need these for DTM */
static uint8_t _dummy_key[CONFIG_HUBBLE_KEY_SIZE];

/* Keep track of the tx task and stop task */
static TaskHandle_t _tx_task_handle;
static TaskHandle_t _stop_task_handle;

/* Sem for tx task, tx interval, shell */
static SemaphoreHandle_t _tx_task_sem;
static SemaphoreHandle_t _tx_interval_sem;
static SemaphoreHandle_t _shell_sem;

/* Timer for tx interval */
static esp_timer_handle_t _tx_interval_timer;

/* Application / DTM specific states */
static enum hubble_sat_dtm_packet_type _packet_type = HUBBLE_SAT_DTM_PACKET_0;
static uint32_t _interval_ms = 1000; /* Default to 1s interval */
static uint8_t _channel = HUBBLE_DEFAULT_CHANNEL;
static bool _is_transmitting = false;
static bool _sweep = false;

static void _tx_interval_timer_cb(void *arg)
{
	(void)arg;
	xSemaphoreGive(_tx_interval_sem);
}

static void _transmit_task(void *arg)
{
	(void)arg;

	TaskHandle_t stop_task;
	uint32_t tx_interval;
	bool sweep;

	while (true) {
		/* wait for transmit command */
		xSemaphoreTake(_tx_task_sem, portMAX_DELAY);

		xSemaphoreTake(_shell_sem, portMAX_DELAY);
		/* Copy global -> local */
		tx_interval = _interval_ms;
		sweep = _sweep;
		_is_transmitting = true;
		xSemaphoreGive(_shell_sem);

		/* Re-init count to 1 by 1st reset the sem and give it */
		xSemaphoreTake(_tx_interval_sem, 0);
		xSemaphoreGive(_tx_interval_sem);

		while (true) {
			xSemaphoreTake(_shell_sem, portMAX_DELAY);
			if (!_is_transmitting) {
				break;
			}
			xSemaphoreGive(_shell_sem);

			/*
			 * Don't keep both the shell sem and the transmit sem
			 * because the transmit interval could be very long,
			 * and blocking the shell.
			 *
			 * But this turns into a risk if the shell takes too
			 * long, it'll mess up the interval. Unless absolute
			 * interval is needed, this should be fine.
			 */
			xSemaphoreTake(_tx_interval_sem, portMAX_DELAY);
			xSemaphoreTake(_shell_sem, portMAX_DELAY);

			/* Just double check once again, bc when tx stops, it
			 * release the transmit sem immdeiately, and we don't
			 * want anymore tx.
			 */
			if (!_is_transmitting) {
				break;
			}

			if (hubble_sat_dtm_packet_send(
				    _packet_type, sweep ? -1 : _channel) != 0) {
				printf("Failed to send packet\n");
			}

			/* One shot timer */
			if (esp_timer_start_once(_tx_interval_timer,
						 tx_interval * 1000U) != ESP_OK) {
				printf("Failed to start timer\n");
				_is_transmitting = false;
				break;
			}

			xSemaphoreGive(_shell_sem);
		}

		/*
		 * Get the task that is waiting for this to stop,
		 * since we still hold the shell sem, we can be sure that
		 * no other task is modifying the stop task handle
		 */
		stop_task = _stop_task_handle;
		_stop_task_handle = NULL;

		xSemaphoreGive(_shell_sem);

		if (stop_task != NULL) {
			xTaskNotifyGive(stop_task);
		}
	}

	/* Unreachable */
	_tx_task_handle = NULL;
	vTaskDelete(NULL);
}

static int _cmd_carrier_wave(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	return hubble_sat_dtm_cw_start(_channel);
}

static int _cmd_power(int argc, char **argv)
{
	int ret;
	int power;

	if (argc != 2) {
		printf("Usage: power <power_dbm>\n");
		return -EINVAL;
	}

	power = atoi(argv[1]);
	ret = hubble_sat_dtm_power_set((int8_t)power);
	if (ret < 0) {
		printf("Failed to set power\n");
		return ret;
	}

	return 0;
}

static int _cmd_channel(int argc, char **argv)
{
	int channel;

	if (argc != 2) {
		printf("Usage:  channel <0..%d>\n", HUBBLE_NUM_CHANNELS - 1);
		return -EINVAL;
	}

	channel = atoi(argv[1]);
	if (channel < 0 || channel >= HUBBLE_NUM_CHANNELS) {
		printf("Invalid channel\n");
		return -EINVAL;
	}

	_channel = (uint8_t)channel;
	return 0;
}

static int _cmd_stop(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	xSemaphoreTake(_shell_sem, portMAX_DELAY);

	/* Check if it's a packet tx loop or just cw */
	if (_is_transmitting) {
		_is_transmitting = false;

		_stop_task_handle = xTaskGetCurrentTaskHandle();

		/*
		 * if interval is too long, the thread blocks on transmit sem,
		 * so release it immediately to exit tx.
		 */
		(void)esp_timer_stop(_tx_interval_timer);
		xSemaphoreGive(_tx_interval_sem);

		xSemaphoreGive(_shell_sem);

		/* Block until tx task exits the tx loop */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

	} else {
		(void)hubble_sat_dtm_cw_stop();
		xSemaphoreGive(_shell_sem);
	}

	return 0;
}

static int _cmd_payload(int argc, char **argv)
{
	int32_t len;

	if (argc != 2) {
		goto error;
	}

	len = strtol(argv[1], NULL, 10);

	switch (len) {
	case -1:
		_packet_type = HUBBLE_SAT_DTM_PACKET_SINGLE_FRAME;
		break;
	case 0:
		_packet_type = HUBBLE_SAT_DTM_PACKET_0;
		break;
	case 4:
		_packet_type = HUBBLE_SAT_DTM_PACKET_4;
		break;
	case 9:
		_packet_type = HUBBLE_SAT_DTM_PACKET_9;
		break;
	case 13:
		_packet_type = HUBBLE_SAT_DTM_PACKET_13;
		break;
	default:
		goto error;
		break;
	}

	return 0;

error:
	printf("Failed to set payload len. Valid options are: "
	       "-1, 0, 4, 9 or 13\n");
	return -EINVAL;
}

static int _cmd_transmit(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	return hubble_sat_dtm_packet_send(_packet_type, _channel);
}

static int _cmd_transmit_repeating(int argc, char **argv, bool sweep)
{
	int interval;

	if (argc != 2) {
		printf("Usage: transmit_repeating <interval_ms>\n");
		return -EINVAL;
	}

	interval = atoi(argv[1]);
	if (interval < 0) {
		printf("Invalid interval\n");
		return -EINVAL;
	}

	/*
	 * Interval 0 = transmit back to back,
	 * so just set it a number < packet tx time
	 */
	if (interval == 0) {
		interval = 100;
	}

	xSemaphoreTake(_shell_sem, portMAX_DELAY);

	/* Check first before setting the shared variables */
	if (_is_transmitting) {
		printf("Transmission already in progress, use stop to finish "
		       "it first\n");
		xSemaphoreGive(_shell_sem);
		return -EBUSY;
	}

	_interval_ms = (uint32_t)interval;
	_sweep = sweep;

	xSemaphoreGive(_tx_task_sem);
	xSemaphoreGive(_shell_sem);

	return 0;
}

static int _cmd_transmit_continuously(int argc, char **argv)
{
	return _cmd_transmit_repeating(argc, argv, false);
}

static int _cmd_transmit_sweep(int argc, char **argv)
{
	return _cmd_transmit_repeating(argc, argv, true);
}

static void _register_commands(void)
{
	const esp_console_cmd_t carrier_wave_cmd = {
		.command = "wave",
		.help = "Start transmitting a carrier wave on the specified "
			"channel",
		.func = _cmd_carrier_wave,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&carrier_wave_cmd));

	const esp_console_cmd_t power_cmd = {
		.command = "power",
		.help = "Set the transmission power in dBm",
		.hint = "<power_dbm>",
		.func = _cmd_power,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&power_cmd));

	const esp_console_cmd_t channel_cmd = {
		.command = "channel",
		.help = "Set the transmission channel",
		.hint = "<channel>",
		.func = _cmd_channel,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&channel_cmd));

	const esp_console_cmd_t stop_cmd = {
		.command = "stop",
		.help = "Stop any ongoing transmission",
		.func = _cmd_stop,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&stop_cmd));

	const esp_console_cmd_t payload_cmd = {
		.command = "payload",
		.help = "Set the payload len for packet transmission (-1, 0, "
			"4, 9, 13)",
		.func = _cmd_payload,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&payload_cmd));

	const esp_console_cmd_t transmit_cmd = {
		.command = "transmit",
		.help = "Transmit a single packet with the current settings",
		.func = _cmd_transmit,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&transmit_cmd));

	const esp_console_cmd_t transmit_continuously_cmd = {
		.command = "transmit_continuously",
		.help = "Start continuous transmission of packets at the set "
			"interval",
		.func = _cmd_transmit_continuously,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&transmit_continuously_cmd));

	const esp_console_cmd_t transmit_sweep_cmd = {
		.command = "transmit_sweep",
		.help = "Start sweeping transmission across channels at the "
			"set interval",
		.func = _cmd_transmit_sweep,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&transmit_sweep_cmd));
}

static esp_err_t _shell_init(void)
{
	esp_console_repl_t *repl = NULL;
	esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	repl_config.prompt = "hubble>";
	repl_config.max_cmdline_length = 256;

	/* Register commands */
	esp_console_register_help_command();
	_register_commands();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) ||                                \
	defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
	esp_console_dev_uart_config_t hw_config =
		ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(
		esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
	esp_console_dev_usb_cdc_config_t hw_config =
		ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(
		esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
	esp_console_dev_usb_serial_jtag_config_t hw_config =
		ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(
		&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

	return esp_console_start_repl(repl);
}

void app_main(void)
{
	esp_err_t ret;
	BaseType_t rc;

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
	    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Init Bluetooth (NimBLE) */
	ret = nimble_port_init();
	if (ret != ESP_OK) {
		ESP_LOGE(APP_TAG, "Failed to init NimBLE stack, error: %d", ret);
		return;
	}

	/* Init the sems */
	_tx_task_sem = xSemaphoreCreateBinary();
	_tx_interval_sem = xSemaphoreCreateBinary();
	_shell_sem = xSemaphoreCreateBinary();

	if (_tx_task_sem == NULL || _tx_interval_sem == NULL ||
	    _shell_sem == NULL) {
		ESP_LOGE(APP_TAG, "Failed to create semaphores");
		return;
	}

	/* Make count of shell sem to 1 initially */
	xSemaphoreGive(_shell_sem);

	/* Create the timer */
	esp_timer_create_args_t tx_timer_args = {
		.callback = _tx_interval_timer_cb,
		.name = "tx_interval_timer",
	};

	ret = esp_timer_create(&tx_timer_args, &_tx_interval_timer);
	if (ret != ESP_OK) {
		ESP_LOGE(APP_TAG, "Failed to create timer");
		return;
	}

	/*
	 * We don't need real key and time since we don't have any
	 * requirement for the payload content.
	 */
	if (hubble_init(0U, _dummy_key) != 0) {
		ESP_LOGE(APP_TAG, "Failed to initialize Hubble Network");
		return;
	}

	/* Create the tx task */
	rc = xTaskCreate(_transmit_task, "transmit_task", TX_STACK_SIZE, NULL,
			 5, &_tx_task_handle);
	if (rc != pdPASS) {
		ESP_LOGE(APP_TAG, "Failed to create transmit task");
		return;
	}

	/* Start the shell */
	ret = _shell_init();
	if (ret != ESP_OK) {
		ESP_LOGE(APP_TAG, "Failed to initialize shell");
	}
}
