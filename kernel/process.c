#include <stdint.h>
#include <stdbool.h>
#include "util/mem.h"
#include "process.h"
#include "mmu.h"
#include "user.h"

uint32_t process_bitmap[MAX_PROCESSES/32];
Process *process[MAX_PROCESSES];

pid_t process_create(uid_t uid, gid_t gid) {
	Process *proc;
	pid_t pid;
	uint32_t entry;
	int i, j;
	
	for(i = 0; i < MAX_PROCESSES/32; i++)
		if((entry = process_bitmap[i]) != 0xFFFFFFFFUL)
			goto find_bit;
	return 0;
	
	find_bit:
	for(j = 0; (entry & 0x1); entry >>= 1, j++);
	pid = i*32 + j;
	process_bitmap[i] |= (1 << j);
	proc = kmalloc(sizeof(Process));
	memset(proc, 0, sizeof(Process));
	
	proc->pid = pid;
	proc->user = uid;
	proc->group = gid;
	//TODO: system_get_ticks()
	proc->time_started = 1337;
	
	//TODO: allocate segments perhaps? or let caller do it later
	//TODO: set stdin/out/err? or let caller do later
	
	process[pid] = proc;
	return pid;
}

void process_kill() {
	
}

void process_switch_to(pid_t pid) {
	mmu_set_crp(&process[pid]->page_table);
	
}

Process *scheduler(uint32_t status_reg, void *stack_pointer, void *program_counter) {
	static pid_t current_process;
	uint32_t i, j, bits;
	
	process[current_process]->status_reg = status_reg;
	process[current_process]->stack_pointer = stack_pointer;
	process[current_process]->program_counter = program_counter;
	
	for(i = ((current_process + 1) % MAX_PROCESSES)/(MAX_PROCESSES/32); !process_bitmap[i]; i = (i + 1) % (MAX_PROCESSES/32));
	bits = process_bitmap[i];
	for(j = 0; !(bits & 0x1); bits >>=1, j++);
	
	current_process = i*32 + j;
	
	mmu_set_crp(&process[current_process]->page_table);
	//TODO: set up stack pointers, load registers
	//TODO: run process
	return process[current_process];
}
