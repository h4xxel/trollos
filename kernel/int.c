#include <chipset.h>
#include <mem_addr.h>
#include "printf.h"

extern void *int_stub;
extern void *int_stub_bus_error;
extern void int_enable();

void int_init() {
	volatile void **int_vec = (volatile void **) 0x60;
	//volatile unsigned int *io_port = (MEM_CHIPSET_SPACE);
	int i;

	for (i = 0; i < 15; i++)
		int_vec[i] = &int_stub;
	int_vec[CHIPSET_INT_NUM_VGA_VSYNC] = &int_stub;
	printf("interrupt stub: %p\n",  &int_stub);
	__asm__("mov.w 0x3000, %sr");
	//*io_port = 1;
	return;
}

void int_stub_handle() {
	volatile int *addr = (volatile void *) 0x20000004;

	//terminal_putc_simple('.');
	*addr = 0;
}