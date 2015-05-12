#include <chipset.h>
#include <mem_addr.h>
#include <syscall.h>
#include "int.h"
#include "log.h"
#include "kernel.h"

void **int_vector = (void *) MEM_LLRAM;

void int_init() {
	int i;
	
	int_move_vector((void *) MEM_LLRAM);
	kprintf(LOG_LEVEL_INFO, "Interrupt vector is at 0x%X\n", int_vector);
	
	for (i = 2; i < 15; i++)
		int_vector[i] = int_stub_bus_error;
	
	for(i = 0; i < 16; i++)
		int_vector[INT_OFFSET_TRAP + i] = int_invalid_trap;
	
	int_vector[SYSCALL_TRAP + INT_OFFSET_TRAP] = int_syscall;
	
	int_vector[CHIPSET_INT_BASE + CHIPSET_INT_NUM_VGA_VSYNC] = int_stub;
	int_enable();
	*CHIPSET_IO(CHIPSET_IO_PORT_INTERRUPT_ENABLE) = 1;
}

void int_stub_handle() {
	
	*CHIPSET_IO(CHIPSET_IO_PORT_IRQ_ACK_VGA) = 0;
}
