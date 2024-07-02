// SPDX-License-Identifier: GPL-2.0+
/*
 * Board functions for Sysam AMCORE (MCF5307 based) board
 *
 * (C) Copyright 2016  Angelo Dureghello <angelo@sysam.it>
 *
 * This file copies memory testdram() from sandburst/common/sb_common.c
 */

#include <common.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/immap.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/platform_data/serial_coldfire.h>

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	puts("Board: ");
	puts("Tenkon v.001(alpha)\n");

	return 0;
}

#define DRAM_CFG_BASE 0xF8000000

int dram_init(void)
{
	debug("dram_init: baudrate=%d\n", gd->baudrate);

	/* TODO: magic numbers */
	const uint32_t C = 0b00101100000;
	const uint32_t R = 0b00111010111;
	const uint32_t B = 0b11;
	const uint32_t EC0 = 0;

	const uint32_t byte_offset = EC0 ? 0 : 3;
	/* Map the configuration words to the address lines */
	const uintptr_t cfg_addr = DRAM_CFG_BASE | (R << 15) | (B << 13) | (C << 2) | byte_offset;

	/* Configure the DRAM chip by accessing the specified address. The value written
	 * doesn't matter but the PLDs are expected a write.
	 */
	*(volatile uint8_t *)cfg_addr = 0;

	/* Delay. Must be at least 60 ms */
	for (uint32_t i = 0; i < 100000; i++)
		asm("nop");

	gd->ram_size = get_ram_size((long *)CFG_SYS_SDRAM_BASE,
				    CFG_SYS_SDRAM_SIZE);

	return 0;
}

// TODO timer
unsigned long timer_read_counter(void)
{
	return 0;
}