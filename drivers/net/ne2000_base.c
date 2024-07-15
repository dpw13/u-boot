/*
Ported to U-Boot by Christian Pellegrin <chri@ascensit.com>

Based on sources from the Linux kernel (pcnet_cs.c, 8390.h) and
eCOS(if_dp83902a.c, if_dp83902a.h). Both of these 2 wonderful world
are GPL, so this is, of course, GPL.

==========================================================================

dev/if_dp83902a.c

Ethernet device driver for NS DP83902a ethernet controller

==========================================================================
####ECOSGPLCOPYRIGHTBEGIN####
-------------------------------------------
This file is part of eCos, the Embedded Configurable Operating System.
Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.

eCos is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 or (at your option) any later version.

eCos is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with eCos; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

As a special exception, if other files instantiate templates or use macros
or inline functions from this file, or you compile this file and link it
with other works to produce a work based on this file, this file does not
by itself cause the resulting work to be covered by the GNU General Public
License. However the source code for this file must still be made available
in accordance with section (3) of the GNU General Public License.

This exception does not invalidate any other reasons why a work based on
this file might be covered by the GNU General Public License.

Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
at http://sources.redhat.com/ecos/ecos-license/
-------------------------------------------
####ECOSGPLCOPYRIGHTEND####
####BSDCOPYRIGHTBEGIN####

-------------------------------------------

Portions of this software may have been derived from OpenBSD or other sources,
and are covered by the appropriate copyright disclaimers included herein.

-------------------------------------------

####BSDCOPYRIGHTEND####
==========================================================================
#####DESCRIPTIONBEGIN####

Author(s):	gthomas
Contributors:	gthomas, jskov, rsandifo
Date:		2001-06-13
Purpose:
Description:

FIXME:		Will fail if pinged with large packets (1520 bytes)
Add promisc config
Add SNMP

####DESCRIPTIONEND####

==========================================================================
*/

#define LOG_DEBUG
#define DEBUG

#include <common.h>
#include <command.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/ofnode.h>
#include <dm/read.h>
#include <env.h>
#include <irq_func.h>
#include <log.h>
#include <net.h>
#include <malloc.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/ethtool.h>
#include <linux/types.h>
#include <stdint.h>

/* NE2000 base header file */
#include "ne2000_base.h"

#if defined(CONFIG_DRIVER_AX88796L)
/* AX88796L support */
#include "ax88796.h"
#else
/* Basic NE2000 chip support */
#include "ne2000.h"
#endif

/* To avoid corruption of register pages or DMA, we need to 
 * ensure that only one thread (the interrupt handler or
 * userspace) is accessing the registers at a time.
 * To could disable global interrupts but disabling interrupts
 * on this specific hardware is more targeted and should be lower
 * jitter.
 */
static inline void dp83902a_lock(dp83902a_priv_data_t *dp, const char *func)
{
	u8* base = dp->base;
	u8 lock = dp->lock++;

	/* Only change state if we weren't previously locked. 
	 * We have to be very careful here that we are incrementing the
	 * lock atomically and retrieving the result of that increment.
	 * If we print the lock state later we risk have inconsistent
	 * results.
	 */
	if (lock == 0) {
		/* Use the IRQ mask as a proxy for whether we're running */
		if (dp->irq_mask) {
			DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
		} else {
			DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA);
		}
		/* The interrupt mask can be read from P2 but instead of 
		 * expending multiple bus cycles to do so, we let software
		 * control the IRQ mask independently and set it in unlock
		 * below.
		 */
		DP_OUT(base, DP_IMR, 0x00);		/* Disable all interrupts */
		asm volatile("nop");
		asm volatile("nop");
	}

	/* We don't wait for the lock like a normal lock, because
	 * the ISR is the only other user of this lock. This is
	 * really just an indicator that userspace is in a critical
	 * section. In fact we allow one critical section to call
	 * another, so we only allow interrupts if we're in no
	 * critical sections.
	 */
#if NE2K_DEBUG & NE2K_DEBUG_LOCK
	debug("dp83902a_lock %s: %d %02x\n", func, lock,  dp->irq_mask);
#endif
}

static inline void dp83902a_unlock(dp83902a_priv_data_t *dp, const char *func)
{
	u8* base = dp->base;
	/* Predecrement and compare to zero to determine if the lock was released */
	u8 lock = --dp->lock;

	if(lock == 0) {
		/* The lock count has dropped to zero: re-enable interrupts */
		if (dp->irq_mask) {
			DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
		} else {
			DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA);
		}
		/* Restore interrupt status */
		DP_OUT(base, DP_IMR, dp->irq_mask);
	}
#if NE2K_DEBUG & NE2K_DEBUG_LOCK
	debug("dp83902a_unlock %s: %d %02x\n", func, lock, dp->irq_mask);
#endif
}

/* This is the default startup value for the RTL8019AS */
static const u8 default_mac[6] = {0x00, 0x00, 0x40, 0x00, 0x00, 0x00};
/**
 * This function reads the MAC address from the serial EEPROM,
 * used if PROM read fails. Does nothing for ax88796 chips (sh boards)
 */
static int
dp83902a_get_ethaddr(dp83902a_priv_data_t *dp, unsigned char *enetaddr)
{
	u8* base = dp->base;
#if defined(NE2000_BASIC_INIT)
	int i;
#endif

	DEBUG_FUNCTION();

	if (!base)
		return -ENODEV;	/* No device found */

#if defined(NE2000_BASIC_INIT)
	/* AX88796L doesn't need */

	dp83902a_lock(dp, __func__);
	/* Prepare ESA */
	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE1);	/* Select page 1 */
	/* Use the address from the serial EEPROM */
	for (i = 0; i < 6; i++)
		DP_IN(base, DP_P1_PAR0+i, dp->esa[i]);
	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE0);	/* Select page 0 */
	dp83902a_unlock(dp, __func__);

	printk("NE2000 - %s ESA: %02x:%02x:%02x:%02x:%02x:%02x\n",
		"eeprom",
		dp->esa[0],
		dp->esa[1],
		dp->esa[2],
		dp->esa[3],
		dp->esa[4],
		dp->esa[5] );

	if (memcmp(default_mac, dp->esa, 6) == 0) {
		/* This is the startup default and does not consitute an
		 * intentional address. Allow u-boot to select a random
		 * address if configured to do so.
		 */
		return -ENODATA;
	}

	memcpy(enetaddr, dp->esa, 6); /* Use MAC from serial EEPROM */
#endif	/* NE2000_BASIC_INIT */
	return 0;
}

static int
dp83902a_set_ethaddr(dp83902a_priv_data_t *dp, unsigned char *enetaddr)
{
	u8 *base = dp->base;

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE1 | DP_CR_STOP);	/* Select page 1 */
	DP_OUT(base, DP_P1_CURP, RX_START);	/* Current page - next free page for Rx */

	for (int i = 0; i < ETHER_ADDR_LEN; i++) {
		DP_OUT(base, DP_P1_PAR0+i, enetaddr[i]);
	}
	dp83902a_unlock(dp, __func__);

	return 0;
}

static void
dp83902a_stop(dp83902a_priv_data_t *dp)
{
	u8 *base = dp->base;

	DEBUG_FUNCTION();

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP);	/* Brutal */
	DP_OUT(base, DP_ISR, 0xFF);		/* Clear any pending interrupts */
	dp->irq_mask = 0x00;		/* Disable all interrupts */
	dp83902a_unlock(dp, __func__);
}

static void dp83902a_poll(dp83902a_priv_data_t *dp);

/*
 * This function is called to "start up" the interface. It may be called
 * multiple times, even when the hardware is already running. It will be
 * called whenever something "hardware oriented" changes and should leave
 * the hardware ready to send/receive packets.
 */
static void
dp83902a_start(dp83902a_priv_data_t *dp, u8 * enetaddr)
{
	u8 *base = dp->base;

	debug("The MAC is %p\n", enetaddr);

	DEBUG_FUNCTION();

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP); /* Brutal */
	DP_OUT(base, DP_DCR, DP_DCR_INIT);
	DP_OUT(base, DP_RBCH, 0);		/* Remote byte count */
	DP_OUT(base, DP_RBCL, 0);
	DP_OUT(base, DP_RCR, DP_RCR_MON);	/* Accept no packets */
	DP_OUT(base, DP_TCR, DP_TCR_LOCAL);	/* Transmitter [virtually] off */
	DP_OUT(base, DP_TPSR, TX_BUF_1);	/* Transmitter start page */
	dp->tx1 = dp->tx2 = 0;
	dp->tx_next = TX_BUF_1;
	dp->tx_started = false;
	DP_OUT(base, DP_PSTART, RX_START); /* Receive ring start page */
	DP_OUT(base, DP_BNDRY, RX_END - 1); /* Receive ring boundary */
	DP_OUT(base, DP_PSTOP, RX_END);	/* Receive ring end page */
	dp->rx_next = RX_START;
	dp83902a_set_ethaddr(dp, enetaddr);
	/* Enable and start device */
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
	DP_OUT(base, DP_TCR, DP_TCR_NORMAL); /* Normal transmit operations */
	DP_OUT(base, DP_RCR, DP_RCR_AB); /* Accept broadcast, no errors, no multicast */
	dp->broken = 0;

	irq_install_handler(249, (interrupt_handler_t *)dp83902a_poll, dp);

	DP_OUT(base, DP_ISR, 0xFF);		/* Clear any pending interrupts */
	dp->irq_mask = DP_IMR_ALL;		/* Enable all interrupts */
	dp83902a_unlock(dp, __func__);
}

/*
 * This routine is called to start the transmitter. It is split out from the
 * data handling routine so it may be called either when data becomes first
 * available or when an Tx interrupt occurs
 */

static void
dp83902a_start_xmit(dp83902a_priv_data_t *dp, int start_page, int len)
{
	u8 *base = dp->base;

	DEBUG_FUNCTION();

#if NE2K_DEBUG & NE2K_DEBUG_PROCESS
	debug("Tx pkt %d len %d\n", start_page, len);
#endif
	WARN_ON(dp->tx_started);

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_ISR, (DP_ISR_TxP | DP_ISR_TxE));
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
	DP_OUT(base, DP_TBCL, len & 0xFF);
	DP_OUT(base, DP_TBCH, len >> 8);
	DP_OUT(base, DP_TPSR, start_page);
	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);
	dp83902a_unlock(dp, __func__);

	dp->tx_started = true;
}

/* These are the names of the read regs */
static const char* ne2k_p0_reg_names[] = {
	"CR",
	"CLDA0",
	"CLDA1",
	"BNRY",
	"TSR",
	"NCR",
	"FIFO",
	"ISR",
	"CRDA0",
	"CRDA1",
	"ID0",
	"ID1",
	"RSR",
	"CNTR0",
	"CNTR1",
	"CNTR2",
};

static const char* ne2k_p2_reg_names[] = {
	"CR",
	"PSTART",
	"PSTOP",
	"RNPP",
	"TPSR",
	"LNPP",
	"ADDRH",
	"ADDRL",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"RCR",
	"TCR",
	"DCR",
	"IMR",
};

/*
 * Print debugging information regarding the software and hardware
 * states. The CR_START bit is unmodified.
 */
static void dp83902a_debug(dp83902a_priv_data_t *dp) {
	u8 *base = dp->base;
	u8 start_cr;
	u8 new_cr;
	u8 p0_regs[0x10];
	u8 p2_regs[0x10];
	int i;

	dp83902a_lock(dp, __func__);
	DP_IN(base, DP_CR, start_cr);
	new_cr = start_cr & DP_CR_PAGEMSK;

	DP_OUT(base, DP_CR, new_cr | DP_CR_PAGE0 | DP_CR_NODMA);
	for (i=0; i<ARRAY_SIZE(p0_regs); i++) {
		DP_IN(base, i, p0_regs[i]);
	}

	DP_OUT(base, DP_CR, new_cr | DP_CR_PAGE2 | DP_CR_NODMA);
	for (i=0; i<ARRAY_SIZE(p2_regs); i++) {
		DP_IN(base, i, p2_regs[i]);
	}
	dp83902a_unlock(dp, __func__);
 
	printf("NE2K Debug:\n\nP0:\n");
	for (i=0; i<ARRAY_SIZE(p0_regs); i += 2) {
		printf("%-8s: %02x    %-8s: %02x\n",
			ne2k_p0_reg_names[i], p0_regs[i],
			ne2k_p0_reg_names[i+1], p0_regs[i+1]);
	}
	printf("\nP2:\n");
	for (i=0; i<ARRAY_SIZE(p2_regs); i += 2) {
		printf("%-8s: %02x    %-8s: %02x\n",
			ne2k_p2_reg_names[i], p2_regs[i],
			ne2k_p2_reg_names[i+1], p2_regs[i+1]);
	}
	printf("\nDriver:\n");
	printf("    tx1: %02x        tx2: %02x\n", dp->tx1, dp->tx2);
	printf("tx_next: %02x tx_started: %02x\n", dp->tx_next, dp->tx_started);
	printf("rx_next: %02x", dp->rx_next);
}

/*
 * In some cases the hardware may enter a bad state, such
 * as with the local DMA getting jammed. This function attempts
 * to recover from such conditions with minimal invasiveness.
 * Note that packet loss is expected.
 */
static void dp83902a_recover(dp83902a_priv_data_t *dp) {
	u8 *base = dp->base;

	/* First off, stop operations (soft reset) */
	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_STOP | DP_CR_NODMA);
	/* Do not accept or transmit new packets */
	DP_OUT(base, DP_RCR, DP_RCR_MON);	/* Accept no packets */
	DP_OUT(base, DP_TCR, DP_TCR_LOCAL);	/* Transmitter [virtually] off */

	CYGACC_CALL_IF_DELAY_US(2000);

	/* Reset remote DMA */
	DP_OUT(base, DP_RBCH, 0);
	DP_OUT(base, DP_RBCL, 0);

	/* Reinitialize RX ring buffer */
	DP_OUT(base, DP_PSTART, RX_START); /* Receive ring start page */
	DP_OUT(base, DP_BNDRY, RX_END - 1); /* Receive ring boundary */
	DP_OUT(base, DP_PSTOP, RX_END);	/* Receive ring end page */
	dp->rx_next = RX_START;

	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE1 | DP_CR_STOP);	/* Select page 1 */
	DP_OUT(base, DP_P1_CURP, RX_START);	/* Current page - next free page for Rx */

	/* Enable and start device */
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_STOP | DP_CR_NODMA);
	DP_OUT(base, DP_TCR, DP_TCR_NORMAL); /* Normal transmit operations */
	DP_OUT(base, DP_RCR, DP_RCR_AB); /* Accept broadcast, no errors, no multicast */

	DP_OUT(base, DP_ISR, 0xFF);		/* Clear any pending interrupts */
	dp->broken = 0;
	dp->irq_mask = DP_IMR_ALL;		/* Enable all interrupts */
	dp83902a_unlock(dp, __func__);
}

/*
 * This routine is called to send data to the hardware. It is known a-priori
 * that there is free buffer space (dp->tx_next).
 */
static void
dp83902a_send(dp83902a_priv_data_t *dp, u8 *data, int total_len)
{
	u8 *base = dp->base;
	u8 start_page, isr;
	u16 len, pkt_len;
#if NE2K_DEBUG & NE2K_DEBUG_IO
	int dx = 0;
#endif

	DEBUG_FUNCTION();

	len = pkt_len = total_len;
	if (pkt_len < IEEE_8023_MIN_FRAME)
		pkt_len = IEEE_8023_MIN_FRAME;

	start_page = dp->tx_next;
	/* Ping-pong transmit buffers.
	 * DPW 7/12/24: I have no idea why this is necessary or done this way.
	 */
	if (dp->tx_next == TX_BUF_1) {
		dp->tx1 = TX_BUF_1;
		dp->tx1_len = pkt_len;
		dp->tx_next = TX_BUF_2;
	} else {
		dp->tx2 = TX_BUF_2;
		dp->tx2_len = pkt_len;
		dp->tx_next = TX_BUF_1;
	}

#if NE2K_DEBUG & NE2K_DEBUG_PROCESS
	debug("TX prep page %d len %d\n", start_page, pkt_len);
#endif

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_ISR, DP_ISR_RDC);	/* Clear end of DMA */
	{
		/*
		 * Dummy read. The manual sez something slightly different,
		 * but the code is extended a bit to do what Hitachi's monitor
		 * does (i.e., also read data).
		 * 
		 * According to DP8390D.pdf, this ensures that PRQ is asserted
		 * before the DMA write. It also notes that the remote byte count
		 * should be programmed to a value greater than 1.
		 */

		__maybe_unused u16 tmp;

		DP_OUT(base, DP_RSAL, 0xfe);
		DP_OUT(base, DP_RSAH, (start_page - 1) & 0xff);
		DP_OUT(base, DP_RBCL, 0x02);
		DP_OUT(base, DP_RBCH, 0x00);
		DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_RDMA | DP_CR_START);
		DP_IN_DATA(dp->data, tmp);
		DP_IN_DATA(dp->data, tmp);

		/* RDC doesn't assert here. I don't know why. */
	}

#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_TX_DMA
	/*
	 * Stall for a bit before continuing to work around random data
	 * corruption problems on some platforms.
	 */
	CYGACC_CALL_IF_DELAY_US(10);
#endif

	/* Send data to device buffer(s) */
	DP_OUT(base, DP_RSAL, 0);
	DP_OUT(base, DP_RSAH, start_page);
	DP_OUT(base, DP_RBCL, pkt_len & 0xFF);
	DP_OUT(base, DP_RBCH, pkt_len >> 8);
	DP_OUT(base, DP_CR, DP_CR_WDMA | DP_CR_START);

	/* Put data into buffer */
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
	debug(" sg buf %p len %08x\n ", data, len);
#endif
	while (len > 0) {
#if NE2K_DEBUG & NE2K_DEBUG_IO
		debug(" %02x", *data);
		if (0 == (++dx % 16)) debug("\n ");
#endif

		DP_OUT_DATA(dp->data, *data++);
		len--;
	}
#if NE2K_DEBUG & NE2K_DEBUG_IO
	debug("\n");
#endif
	if (total_len < pkt_len) {
#if NE2K_DEBUG & NE2K_DEBUG_IO
		debug("  + %d bytes of padding\n", pkt_len - total_len);
#endif
		/* Padding to 802.3 length was required */
		for (int i = total_len; i < pkt_len;) {
			i++;
			DP_OUT_DATA(dp->data, 0);
		}
	}

#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_TX_DMA
	/*
	 * After last data write, delay for a bit before accessing the
	 * device again, or we may get random data corruption in the last
	 * datum (on some platforms).
	 */
	CYGACC_CALL_IF_DELAY_US(1);
#endif

	/* Wait for DMA to complete */
	do {
		DP_IN(base, DP_ISR, isr);
	} while ((isr & DP_ISR_RDC) == 0);
	DP_OUT(base, DP_ISR, DP_ISR_RDC);

	/* Then disable DMA */
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

	/* Start transmit if not already going */
	if (!dp->tx_started) {
		if (start_page == TX_BUF_1) {
			dp->tx_int = 1; /* Expecting interrupt from BUF1 */
		} else {
			dp->tx_int = 2; /* Expecting interrupt from BUF2 */
		}
		dp83902a_start_xmit(dp, start_page, pkt_len);
	}
	dp83902a_unlock(dp, __func__);
}

/* Get the number of RX headers pending in memory. */
static inline int dp83902a_rx_hdrs_pending(dp83902a_priv_data_t *dp) {
	WARN_ON(dp->rx_hdr_wr >= RX_HEADER_BUF_SIZE);
	WARN_ON(dp->rx_hdr_rd >= RX_HEADER_BUF_SIZE);
	return (RX_HEADER_BUF_SIZE + dp->rx_hdr_wr - dp->rx_hdr_rd) % RX_HEADER_BUF_SIZE;
}


/*
 * This function is called when a packet has been received. It's job is
 * to prepare to unload the packet from the hardware. Once the length of
 * the packet is known, the upper layer of the driver can be told. When
 * the upper layer is ready to unload the packet, the internal function
 * 'dp83902a_recv' will be called to actually fetch it from the hardware.
 */
static void
dp83902a_RxEvent(dp83902a_priv_data_t *dp, u8 isr)
{
	u8 *base = dp->base;
	u8 hw_wr_pg;
	rx_desc_t *rcv_hdr;
	u8 next_pkt_pg;
	bool discard;
	int i;

	DEBUG_FUNCTION();

	/* Read the current hardware write pointer. We want to read data up
	 * to this point.
	 */
	DP_OUT(base, DP_CR, DP_CR_PAGE1 | DP_CR_NODMA | DP_CR_START);
	DP_IN(base, DP_P1_CURP, hw_wr_pg);
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

	/* We don't read the RSR here because it's not guaranteed to be
	 * coherent with each new header. Instead we use the status
	 * contained in the next header.
	 */

	if (isr & DP_ISR_RxE) {
		/* Error during reception. We don't need to do anything but
		 * clear the interrupt status. If a valid packet was also
		 * accepted, dp83902a_poll will call dp83902a_RxEvent again.
		 */
#if NE2K_DEBUG & NE2K_DEBUG_PROCESS
		debug("Clearing RxE interrupt\n");
#endif
		DP_OUT(base, DP_ISR, DP_ISR_RxE);
		return;
	}

	/* At this point we have read all the volatile registers associated
	 * with this packet, and if another packet comes in and overwrites
	 * these registers before we finish reading out the previous packet
	 * we won't end up with missed packets or data corruption.
	 */
	DP_OUT(base, DP_ISR, DP_ISR_RxP);

	if (hw_wr_pg > RX_END || hw_wr_pg < RX_START) {
		/* Alright, at this point we've detected multiple bad conditions
		 * sequentially. It's time to attempt some recovery. The next time
		 * dp83902a_poll() is called, this `broken` field will cause it to
		 * dump register states and attempt recovery.
		 */
		WARN(1, "Invalid CURR page!");
		dp->broken++;
		return;
	}

	while(dp->rx_next != hw_wr_pg) {
		/* The next packet is guaranteed to be at dp->rx_next because it
		 * is set to the previous packet's next_pkt_pg
		 */
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
		debug("rx hdr at %02x/%02x\n", dp->rx_next, hw_wr_pg);
#endif

		if (dp83902a_rx_hdrs_pending(dp) == (RX_HEADER_BUF_SIZE - 1)) {
			WARN(1, "No space for RX header\n");
			break;
		}

		/* Read incoming packet header */
		WARN_ON(dp->rx_hdr_wr >= RX_HEADER_BUF_SIZE); // Memory corruption...
		rcv_hdr = &dp->rx_hdr_buf[dp->rx_hdr_wr];

		DP_OUT(base, DP_RBCL, sizeof(rx_hdr_t));
		DP_OUT(base, DP_RBCH, 0);
		DP_OUT(base, DP_RSAL, 0);
		DP_OUT(base, DP_RSAH, dp->rx_next);

		DP_OUT(base, DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
		DP_OUT(base, DP_CR, DP_CR_RDMA | DP_CR_START);
#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_RX_DMA
		CYGACC_CALL_IF_DELAY_US(1);
#endif

		for (i = 0; i < sizeof(rx_hdr_t);) {
			DP_IN_DATA(dp->data, ((u8*)rcv_hdr)[i++]);
		}
		/* Only store the remaining byte count and adjust endianness */
		rcv_hdr->pkt_len = le16_to_cpu(rcv_hdr->pkt_len) - sizeof(rcv_hdr);
		/* Store the start page of the packet */
		rcv_hdr->rx_pkt_pg = dp->rx_next;
		/* Only perform read from memory once (Memory corruption...) */
		next_pkt_pg = rcv_hdr->next_pkt_pg;

#if NE2K_DEBUG & NE2K_DEBUG_PACKET
		debug("rx hdr %d at %02x: %02x %02x %04x\n",
			dp->rx_hdr_wr, dp->rx_next, rcv_hdr->rx_status, next_pkt_pg, rcv_hdr->pkt_len);
#endif

		/* Accept only pristine packets or reset to CURR. */
		discard = 0;

		/* Mask out broadcast/multicast bit as it doesn't indicate error */
		if ((rcv_hdr->rx_status & ~DP_RSR_PHY) != 0x01) {
			/* No actual packet was received. In this case the device-local DMA 
			 * should roll back CURR. I believe that the next page ptr in the case
			 * of an error may still represent some size of the packet seen but we
			 * shouldn't do anything with it.
			 */
			debug("discarding header at %02x due to status %02x\n", dp->rx_next, rcv_hdr->rx_status);
		} else if (rcv_hdr->pkt_len > PKTSIZE) {
			debug("discarding header at %02x due to size %04x\n", dp->rx_next, rcv_hdr->pkt_len);
		} else if (next_pkt_pg > RX_END || next_pkt_pg < RX_START) {
			debug("discarding header at %02x due to bogus next pg %02x\n", dp->rx_next, next_pkt_pg);
		} else {
			/* We only get here if the header passes all our tests. */
			/* Increment pointer to indicate a new header is available */
			dp->rx_hdr_wr = (dp->rx_hdr_wr + 1) % RX_HEADER_BUF_SIZE;
			/* Update location of next header */
			dp->rx_next = next_pkt_pg;

			continue;
		}

		/* The header is bad for some reason.
		 * Do not mark this header as available. Even though a valid packet may
		 * still be in memory somewhere, discard it and jump to CURR. Error
		 * recovery on this hardware is not very robust. Because of discards
		 * like this, the BNDRY register may snap *forward* compared to packets
		 * we've actually read, resulting in corrupted data. For this reason
		 * we store the packet start page and only use BNDRY to release memory
		 * back to the hardware.
		 */
		debug("reset rx_next to %02x\n", hw_wr_pg);
		dp->rx_next = hw_wr_pg;
	}
#if NE2K_DEBUG & NE2K_DEBUG_PROCESS
	debug("dp83902a_RxEvent exit with %02x/%02x\n", dp->rx_next, hw_wr_pg);
#endif
}

/*
 * Update the BNDRY register to release packet memory back to hardware.
 * We update the BNDRY register only after entire packets have been read.
 */
static inline void dp83902a_update_boundary(dp83902a_priv_data_t *dp, u8 page) {
	u8 *base = dp->base;

	if (page == RX_START)
		DP_OUT(base, DP_BNDRY, RX_END - 1);
	else
		DP_OUT(base, DP_BNDRY, page - 1);
}

/*
 * This function is called from ne2k_read_pkt below.
 * It's job is to actually fetch data for a packet from the hardware once
 * memory buffers have been allocated for the packet. Note that the buffers
 * may come in pieces, using a scatter-gather list. This allows for more
 * efficient processing in the upper layers of the stack.
 */
static int
dp83902a_recv(dp83902a_priv_data_t *dp, u8 *data)
{
	u8 *base = dp->base;
	/* This is the header of the next packet to retrieve. */
	rx_desc_t *hdr = &dp->rx_hdr_buf[dp->rx_hdr_rd];
	/* Note this may not be valid but we discard it if so */
	u16 len = hdr->pkt_len;
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
	int dx;
#endif

	if (dp83902a_rx_hdrs_pending(dp) == 0) {
		/* No packets pending */
		return 0;
	}

#if NE2K_DEBUG & NE2K_DEBUG_PROCESS
	debug("Rx packet hdr %d status %02x up to 0x%02x00 length %d\n", dp->rx_hdr_rd, hdr->rx_status, hdr->next_pkt_pg, len);
#endif

	/* Invalid packets are discarded upstream; we don't expect to see
	 * broken packets here.
	 */
	if ((hdr->rx_status & DP_RSR_RxP) == 0) {
		/* No valid packet was recieved. Immediately mark this packet as consumed */
		debug("Discarding packet from hdr %d\n", dp->rx_hdr_rd);
		dp->rx_hdr_rd = (dp->rx_hdr_rd + 1) % RX_HEADER_BUF_SIZE;
		return 0;
	}

	dp83902a_lock(dp, __func__);
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

	/* Read incoming packet data */
	DP_OUT(base, DP_RBCL, len & 0xFF);
	DP_OUT(base, DP_RBCH, len >> 8);
	DP_OUT(base, DP_RSAL, 4);		/* Start after header */
	DP_OUT(base, DP_RSAH, hdr->rx_pkt_pg);
	DP_OUT(base, DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
	DP_OUT(base, DP_CR, DP_CR_RDMA | DP_CR_START);
#ifdef CYGHWR_NS_DP83902A_PLF_BROKEN_RX_DMA
	CYGACC_CALL_IF_DELAY_US(10);
#endif

	if (data) {
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
		debug(" sg %02x -> buf %p len %08x \n", hdr->rx_pkt_pg, data, len);
		dx = 0;
#endif
		while (0 < len) {
			u8 tmp;
			DP_IN_DATA(dp->data, tmp);
#if NE2K_DEBUG & NE2K_DEBUG_IO
			debug(" %02x", tmp);
			if (0 == (++dx % 16)) debug("\n ");
#endif
			*data++ = tmp;
			len--;
		}
#if NE2K_DEBUG & NE2K_DEBUG_IO
		debug("\n");
#endif
	}
	/* Restore the length before we release the header */
	len = hdr->pkt_len;

	/* Update the hardware to release packet memory */
	dp83902a_update_boundary(dp, hdr->next_pkt_pg);
	dp83902a_unlock(dp, __func__);

	/* Mark the header slot available */
	dp->rx_hdr_rd = (dp->rx_hdr_rd + 1) % RX_HEADER_BUF_SIZE;

	return hdr->pkt_len;
}

static void dp83902a_TxEvent(dp83902a_priv_data_t *dp, u8 isr)
{
	u8 *base = dp->base;
	__maybe_unused u8 tsr;

	DEBUG_FUNCTION();

	DP_IN(base, DP_TSR, tsr);
	/* TSR is all we need to read to handle a TX event */
	DP_OUT(base, DP_ISR, isr);	/* Clear set bits */

	if (tsr & (DP_TSR_OWC | DP_TSR_ABT | DP_TSR_COL)) {
		dp->tx_status = TX_COLLISION;
	} else if (tsr & (DP_TSR_CRS | DP_TSR_CDH)) {
		dp->tx_status = TX_NO_CARRIER;
	} else if (tsr & (DP_TSR_TxP)) {
		dp->tx_status = TX_OK;
	}

	if (dp->tx_int == 1) {
		dp->tx1 = 0;
	} else {
		dp->tx2 = 0;
	}
	dp->tx_started = false;
	/* Start next packet if one is ready */
	if (dp->tx1) {
		dp83902a_start_xmit(dp, dp->tx1, dp->tx1_len);
		dp->tx_int = 1;
	} else if (dp->tx2) {
		dp83902a_start_xmit(dp, dp->tx2, dp->tx2_len);
		dp->tx_int = 2;
	} else {
		dp->tx_int = 0;
	}
}

/*
 * Read the tally counters. Called in response to a CNT
 * interrupt.
 */
static void
dp83902a_ReadCounters(dp83902a_priv_data_t *dp)
{
	u8 *base = dp->base;
	__maybe_unused u8 cnt1, cnt2, cnt3;

	DP_IN(base, DP_FER, cnt1);
	DP_IN(base, DP_CER, cnt2);
	DP_IN(base, DP_MISSED, cnt3);
	DP_OUT(base, DP_ISR, DP_ISR_CNT);

	dp->frame_err_count += cnt1;
	dp->crc_err_count += cnt2;
	dp->missed_pkt_count += cnt3;
}

/*
 * Deal with an overflow condition. This code follows the procedure set
 * out in section 7.0 of the datasheet.
 */
static void
dp83902a_Overflow(dp83902a_priv_data_t *dp)
{
	u8 *base = dp->base;
	u8 isr;

	/* Issue a stop command and wait 1.6ms for it to complete. */
	DP_OUT(base, DP_CR, DP_CR_STOP | DP_CR_NODMA);
	CYGACC_CALL_IF_DELAY_US(1600);

	/* Clear the remote byte counter registers. */
	DP_OUT(base, DP_RBCL, 0);
	DP_OUT(base, DP_RBCH, 0);

	/* Enter loopback mode while we clear the buffer. */
	DP_OUT(base, DP_TCR, DP_TCR_LOCAL);
	DP_OUT(base, DP_CR, DP_CR_START | DP_CR_NODMA);

	/*
	 * Read in as many packets as we can and acknowledge any and receive
	 * interrupts. Since the buffer has overflowed, a receive event of
	 * some kind will have occurred.
	 */
	DP_IN(base, DP_ISR, isr);
	dp83902a_RxEvent(dp, isr);

	/* Clear the overflow condition and leave loopback mode. */
	DP_OUT(base, DP_ISR, DP_ISR_OFLW);
	DP_OUT(base, DP_TCR, DP_TCR_NORMAL);

	/*
	 * If a transmit command was issued, but no transmit event has occurred,
	 * restart it here.
	 */
	DP_IN(base, DP_ISR, isr);
	if (dp->tx_started && !(isr & (DP_ISR_TxP|DP_ISR_TxE))) {
		DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);
	}
}

static void
dp83902a_poll(dp83902a_priv_data_t *dp)
{
	u8 *base = dp->base;
	u8 isr;

#if NE2K_DEBUG & NE2K_DEBUG_LOCK
	/* We should never see the registers locked */
	WARN_ON(dp->lock);
#endif

	if (dp->broken) {
		dp83902a_debug(dp);
		printk("NE2K: Attempting soft recovery\n");
		dp83902a_recover(dp);
		dp83902a_debug(dp);
	}

	/* This function is called both from the ISR and from regular
	 * code so we protect it with one lock, along with all other
	 * functions called from it.
	 */
	dp83902a_lock(dp, __func__);

	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE0 | DP_CR_START);
	/* Automatically clear any interrupts we don't need to do anything with */
	DP_OUT(base, DP_ISR, DP_ISR_RDC | DP_ISR_RESET);
	DP_IN(base, DP_ISR, isr);

	while (0 != isr) {
		/*
		 * The CNT interrupt triggers when the MSB of one of the error
		 * counters is set.
		 */
		if (isr & DP_ISR_CNT) {
			dp83902a_ReadCounters(dp);
		}

		/*
		 * Check for overflow. It's a special case, since there's a
		 * particular procedure that must be followed to get back into
		 * a running state.
		 */
		if (isr & DP_ISR_OFLW) {
			dp83902a_Overflow(dp);
		} else {
			/*
			 * Check for tx_started on TX event since these may happen
			 * spuriously it seems.
			 */
			if (isr & (DP_ISR_TxP|DP_ISR_TxE) && dp->tx_started) {
				dp83902a_TxEvent(dp, isr & (DP_ISR_TxP|DP_ISR_TxE));
			}
			if (isr & (DP_ISR_RxP|DP_ISR_RxE)) {
				dp83902a_RxEvent(dp, isr & (DP_ISR_RxP|DP_ISR_RxE));
			}
		}
		DP_OUT(base, DP_ISR, DP_ISR_RDC | DP_ISR_RESET);
		DP_IN(base, DP_ISR, isr);
	}

	dp83902a_unlock(dp, __func__);
}


/* U-Boot specific routines */

/* Get the number of RX packets pending in memory. */
static inline int ne2k_rx_pkts_pending(dp83902a_priv_data_t *dp) {
	WARN_ON(dp->rx_pkt_wr >= PKTBUFSRX);
	WARN_ON(dp->rx_pkt_rd >= PKTBUFSRX);
	return (PKTBUFSRX + dp->rx_pkt_wr - dp->rx_pkt_rd) % PKTBUFSRX;
}

/**
 * Setup the driver and init MAC address according to doc/README.enetaddr
 * Called by ne2k_register() before registering the driver @eth layer
 *
 * @param struct ethdevice of this instance of the driver for dev->enetaddr
 * @return 0 on success, -1 on error (causing caller to print error msg)
 */
static int ne2k_eth_probe(struct udevice *dev)
{
	struct eth_pdata *pdata;
	dp83902a_priv_data_t *dp;

	if (dev == NULL)
		return -EINVAL;

	pdata = dev_get_plat(dev);
	if (pdata == NULL)
		return -EINVAL;

	dp = dev_get_priv(dev);
	if (dp == NULL)
		return -EINVAL;

	debug("### ne2k_eth_probe\n");

	pdata->max_speed = 10;
	pdata->phy_interface = PHY_INTERFACE_MODE_INTERNAL;

	if (!dp->base) {
		printk("NE2K must have base assigned\n");
		return -EINVAL;
	}

	dp->data = dp->base + DP_DATA;

	dp->rx_hdr_rd = dp->rx_hdr_wr = 0;
	dp->rx_pkt_rd = dp->rx_pkt_wr = 0;

	return 0;
}

static int ne2k_read_rom_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	int ret;

	ret = get_prom(pdata->enetaddr, dp->base);
	if (!ret) /* get MAC from prom */
		ret = dp83902a_get_ethaddr(dp, pdata->enetaddr);   /* fallback: seeprom */

	return ret;
}

static int ne2k_write_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	return dp83902a_set_ethaddr(dp, pdata->enetaddr);
}

static int ne2k_init(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	dp83902a_start(dp, pdata->enetaddr);
	dp->initialized = 1;
	return 0;
}

static void ne2k_halt(struct udevice *dev)
{
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	debug("### ne2k_halt\n");
	if(dp->initialized)
		dp83902a_stop(dp);
	dp->initialized = 0;
}

static int ne2k_recv(struct udevice *dev, int flags, u8 **packetp)
{
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	int len = 0;
	u8 *buf;

	/* This will service any interrupts and retrieve new packet headers */
	if (flags & ETH_RECV_CHECK_DEVICE) {
		dp83902a_poll(dp);
	}

	/* Do we even have space for a new packet? */
	if (ne2k_rx_pkts_pending(dp) == (PKTBUFSRX-1)) {
		/* This is a retryable error because the packet headers will still
		 * be available when we try again.
		 */
		return -ENOMEM;
	}

	/* Copy to the next available slot in net_rx_packets and record length */
	buf = net_rx_packets[dp->rx_pkt_wr];
	len = dp83902a_recv(dp, buf);

	if (len == 0) {
		/* No pending packets */
		return -EAGAIN;
	}

	WARN_ON(dp->rx_hdr_rd >= RX_HEADER_BUF_SIZE);
	WARN_ON(dp->rx_pkt_wr >= PKTBUFSRX);
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
	debug("ne2k_recv header %d -> pkt %d\n", dp->rx_hdr_rd, dp->rx_pkt_wr);
#endif

	/* PKTBUFSRX is typically a power of 2 so this should compile to
	 * efficient code. */
	dp->rx_pkt_wr = (dp->rx_pkt_wr+1) % PKTBUFSRX;

	/* Set pointer to next packet */
	*packetp = buf;

	return len;
}

static int ne2k_free_pkt(struct udevice *dev, u8 *packet, int length)
{
	/* We make an assumption (which is valid according to current U-Boot docs)
	 * that free_pkt will be called _in_order_.
	 */
	dp83902a_priv_data_t *dp = dev_get_priv(dev);

	/* Warn if we don't think we have any packets in the network stack. */
	WARN_ON(ne2k_rx_pkts_pending(dp) == 0);
#if NE2K_DEBUG & NE2K_DEBUG_PACKET
	//debug("ne2k_free_pkt %d\n", dp->rx_pkt_rd);
#endif

	dp->rx_pkt_rd = (dp->rx_pkt_rd+1) % PKTBUFSRX;
	return 0;
}

static int ne2k_send(struct udevice *dev, void *packet, int length)
{
	dp83902a_priv_data_t *dp = dev_get_priv(dev);
	int tmo;

	//debug("### ne2k_send\n");

	dp->tx_status = TX_BUSY;

	dp83902a_send(dp, (u8 *) packet, length);
	tmo = get_timer(0) + TOUT * CONFIG_SYS_HZ;
	while(1) {
		dp83902a_poll(dp);
		schedule();
		switch(dp->tx_status) {
			case TX_BUSY:
				// keep waiting
				udelay(10);
				break;
			case TX_OK:
				//debug("Packet sucesfully sent\n");
				return 0;
			case TX_COLLISION:
				debug("Collision detected during transmit\n");
				return -EAGAIN;
			case TX_NO_CARRIER:
				debug("Lost carrier during transmit\n");
				return -ENOENT;
			case TX_ERR_FIFO:
				debug("FIFO underflow during transmit\n");
				return -ENODATA;
		}
		if (get_timer(0) >= tmo) {
			printk("transmission error (timeout)\n");
			return -ETIMEDOUT;
		}

	}

	return 0;
}

const char *ne2k_stat_names[] = {
	"Frame alignment errors",
	"CRC errors",
	"Missed packets",
};

static int ne2k_get_sset_count(struct udevice *dev)
{
	return ARRAY_SIZE(ne2k_stat_names);
}

static void ne2k_get_strings(struct udevice *dev, u8 *data)
{
	for (int i = 0; i < ARRAY_SIZE(ne2k_stat_names); i++)
		strcpy(data + i * ETH_GSTRING_LEN, ne2k_stat_names[i]);
}

static void ne2k_get_stats(struct udevice *dev, u64 *data)
{
	dp83902a_priv_data_t *dp = dev_get_priv(dev);

	data[0] = dp->frame_err_count;
	data[1] = dp->crc_err_count;
	data[2] = dp->missed_pkt_count;
}

static int ne2k_eth_of_to_plat(struct udevice *dev)
{
	dp83902a_priv_data_t *priv = dev_get_priv(dev);

	priv->base = dev_remap_addr(dev);

	if(!priv->base)
		return -EINVAL;

	return 0;
}

static const struct eth_ops ne2k_eth_ops = {
	.start	= ne2k_init,
	.send	= ne2k_send,
	.recv	= ne2k_recv,
	.free_pkt = ne2k_free_pkt,
	.stop	= ne2k_halt,
	.write_hwaddr = ne2k_write_hwaddr,
	.read_rom_hwaddr = ne2k_read_rom_hwaddr,
	.get_sset_count = ne2k_get_sset_count,
	.get_strings = ne2k_get_strings,
	.get_stats = ne2k_get_stats,
};

static const struct udevice_id ne2k_eth_ids[] = {
	{ .compatible = "ne,ne2k" },
	{ .compatible = "realtek,rtl8019as" },
	{ }
};

U_BOOT_DRIVER(eth_ne2k) = {
	.name	= "eth_ne2k",
	.id	= UCLASS_ETH,
	.of_match	= ne2k_eth_ids,
	.of_to_plat = ne2k_eth_of_to_plat,
	.probe	= ne2k_eth_probe,
	.ops	= &ne2k_eth_ops,
	.priv_auto	= sizeof(struct dp83902a_priv_data),
	.plat_auto	= sizeof(struct eth_pdata),
};
