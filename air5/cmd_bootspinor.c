/*
 * cmd_bootspinor.c - SPI-NOR OpenWrt boot for SoftBank Air5 (IPQ8072A)
 *
 * 只读 kernel 分区（FIT: kernel+DTB），rootfs 由内核自己挂载。
 * SPI-NOR 读速约 40MB/s，读 8MB kernel 约 200ms，远快于读完整固件。
 *
 * Flash 布局 (W25R512NWEIQ 64MB):
 *   0x000000 ~ 0x310000   启动链     3.06MB  (SBL1/MIBIB/TZ/RPM/APPSBL)
 *   0x310000 ~ 0x400000   间隙        960KB  (留空)
 *   0x400000 ~ 0xC00000   kernel       8MB  ← FIT(kernel+DTB) 写入此处
 *   0xC00000 ~ 0x3C00000  rootfs      48MB  ← squashfs，内核挂载
 *   0x3C00000~ 0x4000000  rootfs_data  4MB  ← jffs2 overlay
 *
 * Boot sequence:
 *   1. sf probe 0
 *   2. sf read 0x44000000 0x400000 0x800000  (只读 8MB kernel)
 *   3. 验证 FIT magic (0xD00DFEED)
 *   4. 设置 bootargs + fdt_high
 *   5. bootm 0x44000000#config@<fit_config>
 *   6. 失败 → 打印 TFTP rescue 步骤
 *
 * rootfs 挂载方式：
 *   优先由 FIT 的 config 节点 /chosen 传递 root= 参数。
 *   若 FIT 未内嵌，通过 bootargs_extra 追加，例如：
 *     setenv bootargs_extra "root=/dev/mtdblock6 rootfstype=squashfs"
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <image.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

/* ── Flash 分区偏移 ───────────────────────────────────────────── */
#define KERNEL_OFFSET   0x00400000UL   /* kernel 起始 (4MB)  */
#define KERNEL_SIZE     0x00800000UL   /* kernel 大小 (8MB)  */
#define ROOTFS_OFFSET   0x00C00000UL   /* rootfs squashfs    */
#define OVERLAY_OFFSET  0x03C00000UL   /* jffs2 overlay      */

/* ── 内存地址（来自 ipq807x.h）────────────────────────────────── */
#define LOAD_ADDR       0x44000000UL   /* CONFIG_SYS_LOAD_ADDR   */
#define FDT_HIGH        0x4A200000UL   /* CONFIG_IPQ_FDT_HIGH    */

/* ── SPI 参数（与 ipq807x.h CONFIG_SF_DEFAULT_* 一致）──────────── */
#define SF_BUS          0
#define SF_CS           0
#define SF_HZ           (48 * 1000 * 1000)
#define SF_MODE         SPI_MODE_0

#define FIT_MAGIC       0xD00DFEEDUL

/* ─────────────────────────────────────────────────────────────── */

static struct spi_flash *air5_probe_flash(void)
{
	struct spi_flash *flash;

	flash = spi_flash_probe(SF_BUS, SF_CS, SF_HZ, SF_MODE);
	if (!flash) {
		printf("bootspinor: ERROR - sf probe failed\n"
		       "  Check W25R512NWEIQ wiring and 1.8V supply\n");
		return NULL;
	}
	printf("bootspinor: %s  %llu MB\n",
	       flash->name, (unsigned long long)flash->size >> 20);
	return flash;
}

static int air5_read_kernel(struct spi_flash *flash)
{
	int ret;

	printf("bootspinor: sf read  0x%08lx + 0x%08lx -> 0x%lx ...",
	       KERNEL_OFFSET, KERNEL_SIZE, LOAD_ADDR);

	ret = spi_flash_read(flash, KERNEL_OFFSET, KERNEL_SIZE,
			     (void *)LOAD_ADDR);
	if (ret)
		printf(" FAILED (%d)\n", ret);
	else
		printf(" OK\n");
	return ret;
}

static int air5_verify_fit(void)
{
	uint32_t magic = be32_to_cpu(*(volatile uint32_t *)LOAD_ADDR);

	if (magic != FIT_MAGIC) {
		printf("bootspinor: bad FIT magic 0x%08x at 0x%lx\n"
		       "  expected 0x%08lx\n"
		       "  kernel partition may be empty or corrupted\n",
		       magic, LOAD_ADDR, FIT_MAGIC);
		return -1;
	}
	printf("bootspinor: FIT magic OK\n");
	return 0;
}

static int air5_bootm(int config)
{
	char addr_arg[48];
	char fdt_str[24];
	const char *base, *extra;
	char args[512];
	char *av[3];

	/* fdt_high: 防止 kernel 将 DTB 搬入 TZ 保留区 */
	snprintf(fdt_str, sizeof(fdt_str), "0x%lx", FDT_HIGH);
	setenv("fdt_high", fdt_str);

	/*
	 * bootargs 组装：
	 * base  = bootargs_base env（或默认值）
	 * extra = bootargs_extra env（追加，用于传 root= 等参数）
	 *
	 * 建议在 OpenWrt DTS 的 chosen 节点里设置 bootargs，
	 * U-Boot 的 bootargs 会与 DTS chosen 合并传给内核。
	 */
	base  = getenv("bootargs_base");
	extra = getenv("bootargs_extra");
	if (!base)
		base = "console=ttyMSM0,115200n8 loglevel=4 rootwait";

	if (extra && extra[0])
		snprintf(args, sizeof(args), "%s %s", base, extra);
	else
		snprintf(args, sizeof(args), "%s", base);

	setenv("bootargs", args);
	printf("bootspinor: bootargs = %s\n", args);

	/* bootm，带 FIT config 选择对应 DTB */
	if (config > 0)
		snprintf(addr_arg, sizeof(addr_arg),
			 "0x%lx#config@%d", LOAD_ADDR, config);
	else
		snprintf(addr_arg, sizeof(addr_arg), "0x%lx", LOAD_ADDR);

	printf("bootspinor: bootm %s\n", addr_arg);

	av[0] = "bootm";
	av[1] = addr_arg;
	av[2] = NULL;
	return do_bootm(NULL, 0, 2, av);   /* 正常不返回 */
}

/* ── 主命令 ───────────────────────────────────────────────────── */

static int do_bootspinor(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	struct spi_flash *flash;
	const char *env;
	char *ep;
	int config = 1;

	printf("\n=== bootspinor: Air5 SPI-NOR Boot ===\n");
	printf("  kernel @ 0x%08lx  size 0x%08lx (%luMB)\n",
	       KERNEL_OFFSET, KERNEL_SIZE, KERNEL_SIZE >> 20);
	printf("  rootfs @ 0x%08lx  (mounted by kernel)\n",
	       ROOTFS_OFFSET);

	env = getenv("fit_config");
	if (env)
		config = (int)simple_strtoul(env, &ep, 10);
	printf("  fit_config = %d\n\n", config);

	/* 1. 探测 SPI Flash */
	flash = air5_probe_flash();
	if (!flash)
		goto rescue;

	/* 2. 只读 kernel 分区（8MB）*/
	if (air5_read_kernel(flash))
		goto rescue;

	/* 3. 验证 FIT magic */
	if (air5_verify_fit())
		goto rescue;

	/* 4. 启动 — 正常不返回 */
	air5_bootm(config);
	printf("bootspinor: bootm returned unexpectedly\n");

rescue:
	printf(
	"\n"
	"=== BOOT FAILED - TFTP RESCUE ===\n"
	"\n"
	"  setenv serverip 192.168.10.1\n"
	"  tftpboot 0x%lx openwrt-air5-kernel.itb\n"
	"  sf probe 0\n"
	"  sf erase 0x%lx 0x%lx\n"
	"  sf write 0x%lx 0x%lx ${filesize}\n"
	"  reset\n\n",
	LOAD_ADDR,
	KERNEL_OFFSET, KERNEL_SIZE,
	LOAD_ADDR, KERNEL_OFFSET);

	if (getenv("serverip")) {
		printf("bootspinor: serverip set, trying auto rescue...\n");
		run_command("run tftp_rescue", 0);
	}

	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	bootspinor, 1, 0, do_bootspinor,
	"Boot OpenWrt from SPI-NOR kernel partition (Air5/IPQ8072A)",
	"\n"
	"  Reads FIT (kernel+DTB) from kernel@0x400000 (8MB).\n"
	"  rootfs squashfs@0xC00000 is mounted by the kernel.\n"
	"\n"
	"  Flash layout:\n"
	"    0x000000  startup chain  3MB   (do not touch)\n"
	"    0x400000  kernel         8MB   <- write FIT here\n"
	"    0xC00000  rootfs        48MB   <- squashfs\n"
	"    0x3C00000 rootfs_data    4MB   <- jffs2 overlay\n"
	"\n"
	"  Env vars:\n"
	"    fit_config    = 1      FIT config index (selects DTB)\n"
	"    bootargs_base = ...    base kernel cmdline\n"
	"    bootargs_extra= ...    appended (e.g. root=/dev/mtdblock6)\n"
	"    serverip      = ...    enables auto TFTP rescue on failure\n"
);
