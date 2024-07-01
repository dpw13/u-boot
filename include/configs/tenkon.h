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

/* This could be SRAM but there's a check in fdtdec.c that expects
 * the gd ptr to be greater than the FDT. If we put gd into SRAM,
 * that check fails.
 */
#define CFG_SYS_INIT_RAM_ADDR	CFG_SYS_SDRAM_BASE
/* size of RAM */
#define CFG_SYS_INIT_RAM_SIZE	CFG_SYS_SDRAM_SIZE

/* memory map space for linux boot data */
#define CFG_SYS_BOOTMAPSZ		(8 << 20)

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

#endif  /* __TENKON_CONFIG_H */
