#ifndef __INT_H__
#define	__INT_H__

#define INT_OFFSET_TRAP 32

void int_stub();
void int_syscall();
void int_stub_bus_error();
void int_init();
void int_enable();
void int_disable();
void int_move_vector(void *destination);

#endif
