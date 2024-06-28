// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright (C) 2004-2007, 2012 Freescale Semiconductor, Inc.
 * TsiChung Liew (Tsi-Chung.Liew@freescale.com)
 */

#include <clock_legacy.h>
#include <asm/global_data.h>
#include <asm/processor.h>

#include <asm/immap.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;
/*
 * get_clocks() fills in gd->cpu_clock and gd->bus_clk
 */
int get_clocks(void)
{
	gd->bus_clk = CFG_SYS_CLK;
	gd->cpu_clk = CFG_SYS_CLK;

	return (0);
}
