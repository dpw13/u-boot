// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright (C) 2004-2007, 2012 Freescale Semiconductor, Inc.
 * TsiChung Liew (Tsi-Chung.Liew@freescale.com)
 */

#include <init.h>
#include <net.h>
#include <vsprintf.h>
#include <command.h>
#include <netdev.h>
#include <asm/global_data.h>

#include <asm/immap.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

int __attribute__((noreturn)) do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	// No reset, just halt
    asm("reset");
	__builtin_unreachable();
}

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	printf("Generic M680x0 class processor\n");

	return 0;
};
#endif /* CONFIG_DISPLAY_CPUINFO */
