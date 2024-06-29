/*
 * Hardware definitions for SC26C92 and compatible dual UARTS.
 * See https://www.nxp.com/docs/en/data-sheet/SC26C92.pdf
 */

#ifndef _SSC26C92_H_
#define _SSC26C92_H_

#include <stdint.h>

struct sc26c92_uart_info {
    uint8_t which; /* 0 for A, 1 for B*/
};

struct sc26c92_serial_plat {
	void __iomem *base;		/* in/out[bwl] */
    struct sc26c92_uart_info *uart_info;
};

typedef struct {
    uint8_t MR;
    union {
        uint8_t SR; // ro
        uint8_t CSR; // wo
    };
    uint8_t CR; // wo
    union {
        uint8_t RxFIFO; // ro
        uint8_t TxFIFO; // wo
    };
} uart_regs_t;

typedef struct {
    uart_regs_t A; // UART A regs
    union {
        uint8_t IPCR; // ro
        uint8_t ACR; // wo
    };
    union {
        uint8_t ISR; // ro
        uint8_t IMR; // wo
    };
    union {
        uint8_t CTU; // ro
        uint8_t CTPU; // wo
    };
    union {
        uint8_t CTL; // ro
        uint8_t CTPL; // wo
    };
    uart_regs_t B; // UART B regs
    uint8_t USER;
    union {
        uint8_t IPR; // ro
        uint8_t OPCR; // wo
    };
    union {
        uint8_t START; // ro
        uint8_t SOP12; // wo
    };
    union {
        uint8_t STOP; // ro
        uint8_t ROP12; // wo
    };
} duart_regs_t;

#define CR_CMD_NONE      (0x0 << 4)
#define CR_CMD_RESET_MR1 (0x1 << 4) // "set pointer" sets to MR1
#define CR_CMD_RESET_RX  (0x2 << 4)
#define CR_CMD_RESET_TX  (0x3 << 4)
#define CR_CMD_RESET_ERR (0x3 << 4)
#define CR_CMD_RESET_MR0 (0xB << 4)

#define CR_ENA_RX (1 << 0)
#define CR_DIS_RX (1 << 1)
#define CR_ENA_TX (1 << 2)
#define CR_DIS_TX (1 << 3)

#define ACR_BAUDGEN_TABLE_0 (0 << 7)
#define ACR_BAUDGEN_TABLE_1 (1 << 7)

#define ACR_CTMODE_COUNTER_IP2      (0 << 4)
#define ACR_CTMODE_COUNTER_TXCA     (1 << 4)
#define ACR_CTMODE_COUNTER_TXCB     (2 << 4)
#define ACR_CTMODE_COUNTER_CLK      (3 << 4)
#define ACR_CTMODE_TIMER_IP2_DIV_1  (4 << 4)
#define ACR_CTMODE_TIMER_IP2_DIV_16 (5 << 4)
#define ACR_CTMODE_TIMER_CLK_DIV_1  (6 << 4)
#define ACR_CTMODE_TIMER_CLK_DIV_16 (7 << 4)

#define CSR_BAUD_EXT2_115_2K    0x6
#define CSR_BAUD_EXT2_57_6K     0x8
#define CSR_BAUD_EXT2_38_4K     0xC
#define CSR_BAUD_EXT2_28_8K     0x4
#define CSR_BAUD_EXT2_9600      0xB
#define CSR_BAUD_TX(x)  ((x) << 0)
#define CSR_BAUD_RX(x)  ((x) << 4)

#define MR0_EN_WDT  (1 << 7)
#define MR0_DIS_WDT (0 << 7)
#define MR0_TX_FULL_THRESH_8 (0 << 4)
#define MR0_TX_FULL_THRESH_4 (1 << 4)
#define MR0_TX_FULL_THRESH_6 (2 << 4)
#define MR0_TX_FULL_THRESH_1 (3 << 4)
#define MR0_BAUD_NORMAL (0)
#define MR0_BAUD_EXT    (1)
#define MR0_BAUD_EXT2   (4)

#define MR1_DIS_RX_RTS  (0 << 7)
#define MR1_EN_RX_RTS   (1 << 7)
#define MR1_PARITY_EVEN (0 << 2)
#define MR1_PARITY_ODD  (1 << 2)
#define MR1_WITH_PARITY  (0 << 3)
#define MR1_FORCE_PARITY (1 << 3)
#define MR1_NO_PARITY    (2 << 3)
#define MR1_BPC_5   0
#define MR1_BPC_6   1
#define MR1_BPC_7   2
#define MR1_BPC_8   3

#define MR2_DIS_TX_RTS  (0 << 5)
#define MR2_EN_TX_RTS   (1 << 5)
#define MR2_DIS_CTS_TX  (0 << 4)
#define MR2_EN_CTS_TX   (1 << 4)

#define MR2_STOP_LEN_1_00   (7)

#endif // _SSC26C92_H_