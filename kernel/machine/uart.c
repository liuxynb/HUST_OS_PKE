// added @lab5_1.

#include <string.h>
#include "uart.h"
//add
#include "util/types.h"
#include "spike_interface/dts_parse.h"

volatile uint32* uart;


void uart_putchar(uint8 ch)
{
    volatile uint32 *status = (void*)(uintptr_t)0x60000008;
    volatile uint32 *tx = (void*)(uintptr_t)0x60000004;
    while (*status & 0x00000008); 
    *tx = ch;
}

int uart_getchar()
{
  volatile uint32 *rx = (void*)(uintptr_t)0x60000000;
  volatile uint32 *status = (void*)(uintptr_t)0x60000008;
  while (!(*status & 0x00000001)); 
  int32_t ch = *rx;
  return ch;
}

struct uart_scan
{
  int compat;
  uint64 reg;
};

static void uart_open(const struct fdt_scan_node *node, void *extra)
{
  struct uart_scan *scan = (struct uart_scan *)extra;
  memset(scan, 0, sizeof(*scan));
}

static void uart_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct uart_scan *scan = (struct uart_scan *)extra;
  if (!strcmp(prop->name, "compatible") && fdt_string_list_index(prop, "sifive,uart0") >= 0) {
    scan->compat = 1;
  } else if (!strcmp(prop->name, "reg")) {
    fdt_get_address(prop->node->parent, prop->value, &scan->reg);
  }
}

static void uart_done(const struct fdt_scan_node *node, void *extra)
{
  struct uart_scan *scan = (struct uart_scan *)extra;
  if (!scan->compat || !scan->reg || uart) return;

  // Enable Rx/Tx channels
  uart = (void*)(uintptr_t)scan->reg;
  uart[UART_REG_TXCTRL] = UART_TXEN;
  uart[UART_REG_RXCTRL] = UART_RXEN;
}

void query_uart(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct uart_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = uart_open;
  cb.prop = uart_prop;
  cb.done = uart_done;
  cb.extra = &scan;

  fdt_scan(fdt, &cb);
}
