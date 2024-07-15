/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Tenkon board configuration
 *
 * (C) Copyright 2016  Angelo Dureghello <angelo@sysam.it>
 */

#ifndef __TENKON_CONFIG_H
#define __TENKON_CONFIG_H

#define CFG_SYS_UART_PORT		0

#define CFG_SYS_CLK			10000000
#define CFG_SYS_CPU_CLK		40000000

/* Definitions data area (in DPRAM) */

#define CFG_SYS_SDRAM_BASE		0x04000000
#define CFG_SYS_SDRAM_SIZE		0x04000000
#define CFG_SYS_FLASH_BASE		0xfff00000

/* Start off in SRAM. If the FDT is loaded from the previous
 * stage (CONFIG_OF_SEPARATE), it must be at a higher location
 * than this. See panic in fdt_find_separate in fdtdec.c.
 */
#define CFG_SYS_INIT_RAM_ADDR	CONFIG_SYS_SRAM_BASE
#define CFG_SYS_INIT_RAM_SIZE	CONFIG_SYS_SRAM_SIZE

/* memory map space for linux boot data */
#define CFG_SYS_BOOTMAPSZ		(16 << 20)

/* Static locations for cache status */
#define ICACHE_STATUS			(CFG_SYS_INIT_RAM_ADDR + \
					 CFG_SYS_INIT_RAM_SIZE - 8)
#define DCACHE_STATUS			(CFG_SYS_INIT_RAM_ADDR + \
					 CFG_SYS_INIT_RAM_SIZE - 4)

// invalidate instruction cache
#define CFG_SYS_ICACHE_INV           (M68K_CACR_CI)
// enable instruction cache
#define CFG_SYS_CACHE_ICACR          (M68K_CACR_EI)

#define CFG_SYS_DCACHE_INV           (M68K_CACR_CD)
#define CFG_SYS_CACHE_DCACR          (M68K_CACR_ED)

/* POST options */
#define CFG_POST (CFG_SYS_POST_MEMORY|CFG_SYS_POST_MEM_REGIONS)
#define CFG_SYS_POST_WORD_ADDR	(0x1000)

/* Boot options */
#define BOOTENV_DEV_NAME_NET(devtypeu, devtypel, instance) \
	"net "
#define BOOTENV_DEV_NET(devtypeu, devtypel, instance) \
	"bootcmd_net=run download_fit; bootm\0"

#define BOOT_TARGET_DEVICES(func) \
	func(NET, net, na)
#include <config_distro_bootcmd.h>

#define CFG_EXTRA_ENV_SETTINGS       \
	"loadaddr=0x06000000\0"		\
	"serverip=192.168.1.125\0"	\
	"bootfile=tenkon.itb\0"	\
	"download_fit=dhcp; tftp ${loadaddr} ${bootfile}\0" \
	BOOTENV

#endif  /* __TENKON_CONFIG_H */
