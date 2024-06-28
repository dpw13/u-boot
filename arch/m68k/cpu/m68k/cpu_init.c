// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2007, 2012 Freescale Semiconductor, Inc.
 * TsiChung Liew (Tsi-Chung.Liew@freescale.com)
 */

#include <cpu_func.h>
#include <init.h>
#include <watchdog.h>
#include <asm/immap.h>
#include <asm/io.h>

#if defined(CONFIG_CMD_NET)
#include <config.h>
#include <net.h>
#include <asm/fec.h>
#endif

/*
 * Breath some life into the CPU...
 *
 * Set up the memory map,
 * initialize a bunch of registers,
 * initialize the UPM's
 */
void cpu_init_f(void)
{
	icache_enable();
}

/*
 * initialize higher level parts of CPU like timers
 */
int cpu_init_r(void)
{
	return (0);
}

void uart_port_conf(int port)
{
	/* Setup Ports: */
	switch (port) {
	case 0:
	case 1:
	case 2:
	case 3:
	default:
		break;
	}

	// TODO
}

#if defined(CONFIG_CMD_NET)
int fecpin_setclear(fec_info_t *info, int setclear)
{
	return 1;
}
#endif
