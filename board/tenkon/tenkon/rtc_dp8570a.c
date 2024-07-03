#include <init.h>
#include <irq_func.h>
#include <time.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include "rtc_dp8570a.h"
#include "irq_vecs.h"

DECLARE_GLOBAL_DATA_PTR;

static volatile uint64_t timestamp = 0;

volatile dp8570a_regs_t* const regs;

void dp8570a_interrupt(void *not_used)
{
    timestamp++;
    // Read period flag register to clear interrupt
    (void)regs->RS0.PFR;
}

int timer_init(void)
{
    regs->MSR = MSR_PAGE(0) | MSR_REGSEL(0);
    regs->RS0.PFR = PFR_OSC_FAIL; // set to 0 when battery is present
    regs->RS0.IRQRR = 0;

    regs->MSR = MSR_PAGE(0) | MSR_REGSEL(1);
    regs->RS1.RTMR = RTMR_XTAL_FREQ(0); // TODO: 0 = 32.768 kHz
    regs->RS1.ICR0 = ICR0_10_MS_IRQ_EN;
    regs->RS1.ICR1 = 0;

	/* initialize and enable timer interrupt */
	irq_install_handler(IRQ_VEC_DP8570A, dp8570a_interrupt, 0);

    return 0;
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On M68K it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}
