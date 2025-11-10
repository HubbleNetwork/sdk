/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include <hubble/hubble_port.h>

#define NVS_PARTITION        storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define SEQUENCE_COUNTER_ID  1U
#define NVS_SECTOR_COUNT     2U

static struct nvs_fs beacon_fs = {
	.flash_device = NVS_PARTITION_DEVICE,
	.offset = NVS_PARTITION_OFFSET,
};

static int beacon_nvs_init(void)
{
	int ret;
	struct flash_pages_info info;

	if (!device_is_ready(beacon_fs.flash_device)) {
		return 0;
	}

	ret = flash_get_page_info_by_offs(beacon_fs.flash_device,
					  beacon_fs.offset, &info);
	if (ret != 0) {
		return ret;
	}

	beacon_fs.sector_size = info.size;
	beacon_fs.sector_count = NVS_SECTOR_COUNT;

	return nvs_mount(&beacon_fs);
}

uint16_t hubble_sequence_counter_get(void)
{
	ssize_t ret;
	uint16_t counter = 0;

	ret = nvs_read(&beacon_fs, SEQUENCE_COUNTER_ID, &counter,
		       sizeof(counter));
	if (ret != sizeof(counter)) {
		if (ret == -ENOENT) {
			goto end;
		}

		return 0;
	}

	if (counter > HUBBLE_BLE_MAX_SEQ_COUNTER) {
		counter = 0;
	}

end:
	(void)nvs_write(&beacon_fs, SEQUENCE_COUNTER_ID,
			&(uint16_t){counter + 1}, sizeof(counter));

	return counter;
}

SYS_INIT(beacon_nvs_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
