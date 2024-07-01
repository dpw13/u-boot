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

int dram_init(void)
{
	// TODO
	gd->ram_size = get_ram_size((long *)CFG_SYS_SDRAM_BASE,
				    CFG_SYS_SDRAM_SIZE);

	return 0;
}

// TODO timer
unsigned long timer_read_counter(void)
{
	return 0;
}