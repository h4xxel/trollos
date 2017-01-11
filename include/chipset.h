#ifndef __INCLUDE_CHIPSET_H__
#define	__INCLUDE_CHIPSET_H__

#include <stdint.h>
#include "mem_addr.h"

#define CHIPSET_INT_BASE 24
enum ChipsetIntNum {
	CHIPSET_INT_NUM_SPI_DONE = 3,
	CHIPSET_INT_NUM_EXTERNAL,
	CHIPSET_INT_NUM_VGA_VSYNC,
	CHIPSET_INT_NUM_AUDIO,
};

#define CHIPSET_IO(x) (((volatile uint32_t *) MEM_CHIPSET_SPACE) + (x))
typedef enum ChipsetIoPort ChipsetIoPort;
enum ChipsetIoPort {
	
	CHIPSET_IO_PORT_IRQ_ACK_SPI = 0x3,
	CHIPSET_IO_PORT_IRQ_ACK_EXTERNAL,
	CHIPSET_IO_PORT_IRQ_ACK_VGA,
	CHIPSET_IO_PORT_IRQ_ACK_AUDIO,
	
	CHIPSET_IO_PORT_GET_RAM_SIZE = 0x10,
	
	CHIPSET_IO_PORT_SPI_MEM_COUNTER = 0x30,
	CHIPSET_IO_PORT_SPI_LINE_SELECT,
	CHIPSET_IO_PORT_SPI_REG,
	
	CHIPSET_IO_PORT_VGA_WINDOW_X = 0x50,
	CHIPSET_IO_PORT_VGA_WINDOW_Y,
	CHIPSET_IO_PORT_VGA_SPRITE_X,
	CHIPSET_IO_PORT_VGA_SPRITE_Y,
	
	CHIPSET_IO_PORT_INTERRUPT_EXTINT = 0x300,
	CHIPSET_IO_PORT_INTERRUPT_ENABLE = 0x301,
	CHIPSET_IO_PORT_INTERRUPT_ACK = 0x302,
	
	CHIPSET_IO_PORT_DEBUG = 0x340,
};

#endif
