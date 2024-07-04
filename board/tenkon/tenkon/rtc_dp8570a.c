#include <init.h>
#include <irq_func.h>
#include <time.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include "rtc_dp8570a.h"
#include "irq_vecs.h"

DECLARE_GLOBAL_DATA_PTR;

static volatile uint64_t timestamp = 0;

volatile dp8570a_regs_t* const regs = (dp8570a_regs_t *)0xFE800000;

void dp8570a_interrupt(void *not_used)
{
	timestamp++;
	// Clear all interrupts
	regs->MSR = MSR_ALL_INT;
}

int timer_init(void)
{
	/* initialize interrupt handler before we start calling interrupts */
	irq_install_handler(IRQ_VEC_DP8570A, dp8570a_interrupt, 0);

	/* Enter test mode and clear test mode register */
	regs->MSR = MSR_PAGE(0) | MSR_REGSEL(0);
	regs->RS0.PFR = PFR_TEST_MODE;
	regs->TEST_MODE = 0;
	regs->RS0.PFR = PFR_SINGLE_SUPPLY; // Change this when battery present

	/* Wait for clock to start up */
	while (regs->RS0.PFR & PFR_OSC_FAIL) {
		regs->MSR = MSR_PAGE(0) | MSR_REGSEL(1);
		regs->RS1.RTMR = RTMR_XTAL_FREQ(0) | RTMR_CLOCK_START; // TODO: 0 = 32.768 kHz
		/* Wait a bit for clock to start and make sure it stays up */
		for (int i=0; i<200000; i++) {
			asm volatile ("nop");
		}
		regs->MSR = MSR_PAGE(0) | MSR_REGSEL(0);
	}

	/* TODO: There are a few standby/powerfail settings not implemented,
	 * see datasheet.
	 */
	regs->MSR = MSR_PAGE(0) | MSR_REGSEL(1);
	/* Now that the oscillator is running, we may have to hit the start
	 * bit one more time. */
	regs->RS1.RTMR = RTMR_XTAL_FREQ(0) | RTMR_CLOCK_START; // TODO: 0 = 32.768 kHz
	/* Configure interrupt pin to push-pull active high */
	regs->RS1.OMR = OMR_INTR_PP | OMR_INTR_NOINVERT;

	switch (CONFIG_SYS_HZ) {
		case 10:
			regs->RS1.ICR0 = ICR0_100_MS_IRQ_EN;
			break;
		case 100:
			regs->RS1.ICR0 = ICR0_10_MS_IRQ_EN;
			break;
		case 1000:
			regs->RS1.ICR0 = ICR0_1_MS_IRQ_EN;
			break;
		default:
			panic("Unhandled CONFIG_SYS_HZ %d\n", CONFIG_SYS_HZ);
	}
	regs->RS1.ICR1 = 0;

	/* Clear any existing interrupts */
	regs->MSR = MSR_ALL_INT;

	/* We don't enable interrupts before this specifically so we don't 
	 * get unhandled interrupts from the RTC until now. */
	enable_interrupts();

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

unsigned long timer_read_counter(void)
{
	return timestamp;
}