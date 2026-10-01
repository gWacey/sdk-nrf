/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <math.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/fs/fs.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdmmc, CONFIG_SD_MMC_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_sdmmc_disk)
#if defined(CONFIG_FAT_FILESYSTEM_ELM)

#include <ff.h>

#if defined(CONFIG_DISK_DRIVER_MMC)
#define DISK_DRIVE_NAME "SD2"
#else
#define DISK_DRIVE_NAME "SD"
#endif

#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t sd_mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = DISK_MOUNT_PT,
};

#elif defined(CONFIG_FILE_SYSTEM_EXT2)

#include <zephyr/fs/ext2.h>

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT	"/ext"

static struct fs_mount_t sd_mp = {
	.type = FS_EXT2,
	.flags = FS_MOUNT_FLAG_NO_FORMAT,
	.storage_dev = (void *)DISK_DRIVE_NAME,
	.mnt_point = "/ext",
};
#endif

static void test_sdcard(void)
{
	int ret;
	struct fs_statvfs st;
	struct fs_dir_t dir;

	LOG_INF("[SD]   mount %s (soft eMMC, 4-bit)", sd_mp.mnt_point);

	ret = fs_mount(&sd_mp);
	if (ret) {
		LOG_ERR("  fs_mount failed (ret = %d): card inserted/formatted?", ret);
		return;
	}

	if (fs_statvfs(sd_mp.mnt_point, &st) == 0) {
		unsigned long total_kb =
			(unsigned long)((uint64_t)st.f_blocks * st.f_frsize / 1024);
		unsigned long free_kb = (unsigned long)((uint64_t)st.f_bfree * st.f_frsize / 1024);

		LOG_INF("  mounted: %lu KB total, %lu KB free", total_kb, free_kb);
	}

	fs_dir_t_init(&dir);

	if (fs_opendir(&dir, sd_mp.mnt_point) == 0) {
		struct fs_dirent ent;
		int n = 0;

		while (fs_readdir(&dir, &ent) == 0 && ent.name[0] != '\0') {
			LOG_INF("   %s %s (%zu)", ent.type == FS_DIR_ENTRY_DIR ? "[D]" : "[F]",
				ent.name, ent.size);
			n++;
		}

		LOG_INF("  %d root entries", n);

		fs_closedir(&dir);
	}

	/* Write/read round-trip to confirm the 4-bit data path end to end. */
	{
		static const char msg[] = "nRF-Audio-EB sEMMC 4-bit round-trip";
		const char *path = DISK_MOUNT_PT "/eb_test.txt";
		char buf[sizeof(msg)] = {0};
		struct fs_file_t file;
		ssize_t wr, rd;

		fs_file_t_init(&file);
		ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
		if (ret) {
			LOG_ERR("  fs_open(%s, write) failed (%d)", path, ret);
		} else {
			wr = fs_write(&file, msg, sizeof(msg));
			(void)fs_close(&file);

			if (wr != (ssize_t)sizeof(msg)) {
				LOG_ERR("  fs_write returned %d (expected %u)", (int)wr,
					(unsigned int)sizeof(msg));
			} else {
				LOG_INF("  wrote %d bytes to %s", (int)wr, path);

				fs_file_t_init(&file);
				ret = fs_open(&file, path, FS_O_READ);
				if (ret) {
					LOG_ERR("  fs_open(%s, read) failed (%d)", path, ret);
				} else {
					rd = fs_read(&file, buf, sizeof(buf));
					(void)fs_close(&file);

					if (rd == (ssize_t)sizeof(msg) &&
					    memcmp(buf, msg, sizeof(msg)) == 0) {
						LOG_INF("  read-back OK (%d bytes): \"%s\"",
							(int)rd, buf);
					} else {
						LOG_ERR("  read-back MISMATCH (rd=%d)", (int)rd);
					}
				}
			}
		}
	}

	fs_unmount(&sd_mp);
}
#else
static void test_sdcard(void)
{
	LOG_WRN("[SD]   not on this board -- skipped");
}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(zephyr_sdmmc_disk) */

int main(void)
{
	LOG_INF("==============================");
	LOG_INF(" QSPI SD/MMC shield smoke test");
	LOG_INF("==============================");

	test_sdcard();

	LOG_INF("===== smoke test complete =====");
	return 0;
}
