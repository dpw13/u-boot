#ifndef __RTC_DP8570A__
#define __RTC_DP8570A__

#include <stdint.h>

typedef struct {
    uint8_t MSR;
    union {
        struct {
            uint8_t T0CR;
            uint8_t T1CR;
            uint8_t PFR;
            uint8_t IRQRR;
        } RS0;
        struct {
            uint8_t RTMR;
            uint8_t OMR;
            uint8_t ICR0;
            uint8_t ICR1;
        } RS1;
    };
    /* Counters */
    uint8_t FRAC_SECS;
    uint8_t SECONDS;
    uint8_t MINUTES;
    uint8_t HOURS;
    uint8_t DAY_OF_MONTH;
    uint8_t MONTHS;
    uint8_t YEARS;
    uint8_t JULIAN_LSB;
    uint8_t JULIAN_MSB;
    uint8_t DAY_OF_WEEK;

    /* Timer data */
    uint8_t TIMER0_LSB;
    uint8_t TIMER0_MSB;
    uint8_t TIMER1_LSB;
    uint8_t TIMER1_MSB;

    /* Time compare RAM */
    uint8_t COMP_SEC;
    uint8_t COMP_MIN;
    uint8_t COMP_HOUR;
    uint8_t COMP_DOM;
    uint8_t COMP_MONTH;
    uint8_t COMP_DOW;

    /* Time save RAM */
    uint8_t SAVE_SEC;
    uint8_t SAVE_MIN;
    uint8_t SAVE_HOUR;
    uint8_t SAVE_DOM;
    uint8_t SAVE_MONTH;

    uint8_t SCRATCH;
    uint8_t TEST_MODE;
} dp8570a_regs_t;

#define MSR_PAGE(x)       ((x) << 7)
#define MSR_REGSEL(x)     ((x) << 6)
#define MSR_TIMER0_INT    (1 << 5)
#define MSR_TIMER1_INT    (1 << 4)
#define MSR_ALARM_INT     (1 << 3)
#define MSR_PERIODIC_INT  (1 << 2)
#define MSR_PWRFAIL_INT   (1 << 1)
#define MSR_INT_STATUS    (1 << 0)
#define MSR_ALL_INT       (MSR_TIMER0_INT | MSR_TIMER1_INT | MSR_ALARM_INT | MSR_PERIODIC_INT | MSR_PWRFAIL_INT)

#define TCR_COUNT_HOLD    (1 << 7)
#define TCR_TMR_READ      (1 << 6)
#define TCR_CLKSEL(x)     ((x) << 3)
#define TCR_MODE(x)       ((x) << 1)
#define TCR_START         (1 << 0)

#define PFR_TEST_MODE     (1 << 7)
#define PFR_OSC_FAIL      (1 << 6)
#define PFR_SINGLE_SUPPLY (1 << 6)
#define PFR_1_MS_FL       (1 << 5)
#define PFR_10_MS_FL      (1 << 4)
#define PFR_100_MS_FL     (1 << 3)
#define PFR_1_S_FL        (1 << 2)
#define PFR_10_S_FL       (1 << 1)
#define PFR_60_S_FL       (1 << 0)

#define IRQRR_TIMESAVE_EN   (1 << 7)
#define IRQRR_LOW_BAT       (1 << 6)
#define IRQRR_PFAIL_DELAY   (1 << 5)
#define IRQRR_TMR1_IRQ      (1 << 4)
#define IRQRR_TMR0_IRQ      (1 << 3)
#define IRQRR_ALARM_IRQ     (1 << 2)
#define IRQRR_PERIODIC_IRQ  (1 << 1)
#define IRQRR_PFAIL_IRQ     (1 << 0)

#define RTMR_XTAL_FREQ(x)   ((x) << 6)
#define RTMR_CLOCK_START    (1 << 3)

#define OMR_MFO_OSC         (1 << 7)
#define OMR_MFO_TMR0        (1 << 6)
#define OMR_MFO_PP          (1 << 5)
#define OMR_MFO_NOINVERT    (1 << 4)
#define OMR_INTR_PP         (1 << 3)
#define OMR_INTR_NOINVERT   (1 << 2)
#define OMR_T1_PP           (1 << 1)
#define OMR_T1_NOINVERT     (1 << 0)

#define ICR0_TMR1_IRQ_EN    (1 << 7)
#define ICR0_TMR0_IRQ_EN    (1 << 6)
#define ICR0_1_MS_IRQ_EN    (1 << 5)
#define ICR0_10_MS_IRQ_EN   (1 << 4)
#define ICR0_100_MS_IRQ_EN  (1 << 3)
#define ICR0_1_S_IRQ_EN     (1 << 2)
#define ICR0_10_S_IRQ_EN    (1 << 1)
#define ICR0_60_S_IRQ_EN    (1 << 0)

#define ICR0_PFAIL_IRQ_EN   (1 << 7)
#define ICR0_ALARM_IRQ_EN   (1 << 6)
#define ICR0_DOW_IRQ_EN     (1 << 5)
#define ICR0_MONTH_IRQ_EN   (1 << 4)
#define ICR0_DOM_IRQ_EN     (1 << 3)
#define ICR0_HOUR_IRQ_EN    (1 << 2)
#define ICR0_MIN_IRQ_EN     (1 << 1)
#define ICR0_SEC_IRQ_EN     (1 << 0)

#endif