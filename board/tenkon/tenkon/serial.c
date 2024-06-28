#include <serial.h>

struct serial_device *default_serial_console(void)
{
	return NULL;
}

#ifdef CONFIG_DEBUG_UART
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int c)
{
}

DEBUG_UART_FUNCS
#endif