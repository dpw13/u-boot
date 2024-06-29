#include <serial.h>
#include <dm.h>
#include <dm/device_compat.h>
#include "serial_sc26c92.h"

// Use readb/writeb

static inline uart_regs_t* get_uart_regs(void __iomem *base, const struct sc26c92_uart_info *uart_info)
{
	duart_regs_t* duart_regs = (duart_regs_t*)base;
	uart_regs_t* uart_regs;

	switch (uart_info->which) {
		case 1:
			uart_regs = &duart_regs->B;
		case 0:
		default:
			uart_regs = &duart_regs->A;
	}

	return uart_regs;
}

static void _sc26c92_serial_init(void __iomem *base,
			       struct sc26c92_uart_info *uart_info)
{
	duart_regs_t* duart_regs = (duart_regs_t*)base;
	uart_regs_t* uart_regs = get_uart_regs(base, uart_info);

    duart_regs->ACR = ACR_BAUDGEN_TABLE_0 | ACR_CTMODE_COUNTER_IP2;
    uart_regs->CR = CR_CMD_RESET_TX;
    uart_regs->CR = CR_CMD_RESET_RX;
    uart_regs->CR = CR_CMD_RESET_MR0;
    uart_regs->MR = MR0_DIS_WDT | MR0_TX_FULL_THRESH_4 | MR0_BAUD_EXT2;
    uart_regs->MR = MR1_EN_RX_RTS | MR1_NO_PARITY | MR1_BPC_8;
    uart_regs->MR = MR2_DIS_TX_RTS | MR2_DIS_CTS_TX | MR2_STOP_LEN_1_00;
    uart_regs->CR = CR_CMD_NONE | CR_ENA_RX | CR_ENA_TX;
}

static void _sc26c92_serial_setbrg(void __iomem *base,
				 struct sc26c92_uart_info *uart_info,
				 int baudrate)
{
	uart_regs_t* uart_regs = get_uart_regs(base, uart_info);
	uint8_t baud_sel;

	/* Disable TX and RX */
	uart_regs->CR = CR_CMD_NONE | CR_DIS_RX | CR_DIS_TX;

	switch(baudrate)
	{
		case 115200:
			baud_sel = CSR_BAUD_EXT2_115_2K;
			break;
		case 57600:
			baud_sel = CSR_BAUD_EXT2_57_6K;
			break;
		case 38400:
			baud_sel = CSR_BAUD_EXT2_38_4K;
			break;
		case 28800:
			baud_sel = CSR_BAUD_EXT2_28_8K;
			break;
		case 9600:
		default:
			baud_sel = CSR_BAUD_EXT2_9600;
			break;
	}

	/* TODO: Technically we should wait until TX has finished the last byte before
	 * changing this.
	 */
    uart_regs->CSR = CSR_BAUD_TX(baud_sel) | CSR_BAUD_RX(baud_sel);
	/* Re-enable */
    uart_regs->CR = CR_CMD_NONE | CR_ENA_RX | CR_ENA_TX;
}

static int sc26c92_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);

	_sc26c92_serial_setbrg(plat->base, plat->uart_info, baudrate);

	return 0;
}

static int sc26c92_serial_setconfig(struct udevice *dev, uint serial_config)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);
	uart_regs_t* uart_regs = get_uart_regs(plat->base, plat->uart_info);
	uint parity = SERIAL_GET_PARITY(serial_config);
	uint bits = SERIAL_GET_BITS(serial_config);
	uint stop = SERIAL_GET_STOP(serial_config);

	/* TODO: Set MR regs based on above */

	return 0;
}

static int _sc26c92_serial_getc(void __iomem *base,
			      struct sc26c92_uart_info *uart_info)
{
	uart_regs_t* uart_regs = get_uart_regs(base, uart_info);

    if ((uart_regs->SR & 0x01) == 0) {
		return -EAGAIN;
	}
    return uart_regs->RxFIFO;
}

static int sc26c92_serial_getc(struct udevice *dev)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);

	return _sc26c92_serial_getc(plat->base, plat->uart_info);
}

static int _sc26c92_serial_putc(void __iomem *base,
			      struct sc26c92_uart_info *uart_info,
			      const char c)
{
	uart_regs_t* uart_regs = get_uart_regs(base, uart_info);

    if ((uart_regs->SR & 0x04) == 0) {
		return -EAGAIN;
	}
    uart_regs->TxFIFO = c;

	return 0;
}

static int sc26c92_serial_putc(struct udevice *dev, const char c)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);

	return _sc26c92_serial_putc(plat->base, plat->uart_info, c);
}

static int sc26c92_serial_pending(struct udevice *dev, bool input)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);
	uart_regs_t* uart_regs = get_uart_regs(plat->base, plat->uart_info);

	if (input)
		return (uart_regs->SR & 0x01) ? 1 : 0;
	else
		return (uart_regs->SR & 0x04) ? 0 : 1;
}

static int sc26c92_serial_probe(struct udevice *dev)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);

	plat->uart_info = (struct sc26c92_uart_info *)dev_get_driver_data(dev);

	_sc26c92_serial_init(plat->base, plat->uart_info);

	return 0;
}

static const struct udevice_id sc26c92_serial_id[] = {
	{ .compatible = "phi,sc26c92-duart" },
};

static int sc26c92_serial_of_to_plat(struct udevice *dev)
{
	struct sc26c92_serial_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = (void __iomem *)addr;

	return 0;
}

static const struct dm_serial_ops sc26c92_serial_ops = {
	.putc = sc26c92_serial_putc,
	.pending = sc26c92_serial_pending,
	.getc = sc26c92_serial_getc,
	.setbrg = sc26c92_serial_setbrg,
	.setconfig = sc26c92_serial_setconfig
};

U_BOOT_DRIVER(serial_sc26c92) = {
	.name = "serial_sc26c92",
	.id = UCLASS_SERIAL,
	.of_match = of_match_ptr(sc26c92_serial_id),
	.of_to_plat = of_match_ptr(sc26c92_serial_of_to_plat),
	.plat_auto	= sizeof(struct sc26c92_serial_plat),
	.ops = &sc26c92_serial_ops,
	.probe = sc26c92_serial_probe,
#if !CONFIG_IS_ENABLED(OF_CONTROL)
	.flags = DM_FLAG_PRE_RELOC,
#endif
};

#ifdef CONFIG_DEBUG_UART
#include <debug_uart.h>

struct sc26c92_uart_info sc26c92_a_info = {
	.which = 0,
};

static inline struct sc26c92_uart_info *_debug_uart_info(void)
{
	return &sc26c92_a_info;
}

static inline void _debug_uart_init(void)
{
	void __iomem *base = (void __iomem *)CONFIG_VAL(DEBUG_UART_BASE);
	struct sc26c92_uart_info *uart_info = _debug_uart_info();

	_sc26c92_serial_init(base, uart_info);
	_sc26c92_serial_setbrg(base, uart_info,
			     CONFIG_BAUDRATE);
}

static inline void _debug_uart_putc(int c)
{
	void __iomem *base = (void __iomem *)CONFIG_VAL(DEBUG_UART_BASE);
	struct sc26c92_uart_info *uart_info = _debug_uart_info();

	while (_sc26c92_serial_putc(base, uart_info, c) == -EAGAIN)
		;
}

DEBUG_UART_FUNCS
#endif