#include <stdint.h>
#include <mem_addr.h>
#include "mem.h"
#include "mmu.h"

extern uint8_t end;

void *ksbrk(intptr_t increment) {
	static void * heap_end = &end;
	uint32_t tmp, new, pages, i;
	
	if(increment > 0) {
		tmp = ((((uint32_t) heap_end) + (MMU_PAGE_SIZE - 1)) & ~MMU_PAGE_MASK);
		new = ((((uint32_t) heap_end) + increment + (MMU_PAGE_SIZE - 1)) & ~MMU_PAGE_MASK);
		pages = (new - tmp)/MMU_PAGE_SIZE;
		
		for(i = 0; i < pages; i++) {
			mmu_alloc_at((void *) (tmp + i*MMU_PAGE_SIZE), true, false);
		}			
	} else if(increment < 0) {
		//TODO: implement
	}
	
	heap_end += increment;
	return heap_end;
}

void *kmalloc(uint32_t size) {
	#warning "Unimplemented function kmalloc"
	return NULL;
}

void kfree(void *pointer) {
	#warning "Unimplemented function kfree"
}

void *memset(void *pointer, int c, uint32_t n) {
	uint8_t *p1 = pointer;
	while(n) {
		p1[--n] = c;
	}
	return NULL;
}

void *memcpy(void *dest, void *src, uint32_t n) {
	uint8_t *p1 = dest, *p2 = src;
	while(n) {
		p1[n] = p2[n];
		n--;
	}
	
	return NULL;
}
