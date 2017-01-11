#include "mmu040.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <mem_addr.h>
#include <chipset.h>
#include <errno.h>
#include "../mmu.h"
#include "../util/mem.h"
#include "../util/log.h"
#include "mmu040.h"

#define SRP_URP_DESCRIPTOR_BITS 9

#define ROOT_LEVEL_DESCRIPTOR_BITS 7
#define ROOT_LEVEL_DESCRIPTORS (1 << ROOT_LEVEL_DESCRIPTOR_BITS)
#define POINTER_LEVEL_DESCRIPTOR_BITS 7
#define POINTER_LEVEL_DESCRIPTORS (1 << POINTER_LEVEL_DESCRIPTOR_BITS)
#define PAGE_LEVEL_DESCRIPTOR_BITS 6
#define PAGE_LEVEL_DESCRIPTORS (1 << PAGE_LEVEL_DESCRIPTOR_BITS)

#define ROOT_LEVEL_ADDR_FIELD(a) ((a) >> (POINTER_LEVEL_DESCRIPTOR_BITS + 2))
#define ROOT_LEVEL_FIELD_ADDR(f) ((f) << (POINTER_LEVEL_DESCRIPTOR_BITS + 2))
#if PAGE_SIZE == 4096
#define POINTER_LEVEL_ADDR_FIELD(a) ((a) >> (PAGE_LEVEL_DESCRIPTOR_BITS + 1))
#define POINTER_LEVEL_FIELD_ADDR(f) ((f) << (PAGE_LEVEL_DESCRIPTOR_BITS + 1))
#else
#define POINTER_LEVEL_ADDR_FIELD(a) ((a) >> (PAGE_LEVEL_DESCRIPTOR_BITS + 2))
#define POINTER_LEVEL_FIELD_ADDR(f) ((f) << (PAGE_LEVEL_DESCRIPTOR_BITS + 2))
#endif
#define PAGE_LEVEL_ADDR_FIELD(a) ((a) >> (PAGE_OFFSET_BITS))
#define PAGE_LEVEL_FIELD_ADDR(f) ((f) << (PAGE_OFFSET_BITS))

#define PAGE_DESCRIPTORS_8K 32
#define PAGE_DESCRIPTORS_4K 64

#define UDT_IS_RESIDENT(x) ((x) == MMU040_UPPER_LEVEL_DESCRIPTOR_TYPE_RESIDENT || (x) == MMU040_UPPER_LEVEL_DESCRIPTOR_TYPE_RESIDENT_ALT)
#define PDT_IS_RESIDENT(x) ((x) == MMU040_PAGE_DESCRIPTOR_TYPE_RESIDENT || (x) == MMU040_PAGE_DESCRIPTOR_TYPE_RESIDENT_ALT)

//Replace the base part of an address (a) with a new base (b)
#define REPAGE(a, b) ((((uint32_t) (b)) & ~MMU_PAGE_MASK) + (((uint32_t) (a)) & (MMU_PAGE_MASK)))

#define MAPPER_PAGES 4

struct {
	uint32_t free_frames;
	uint32_t total_frames;
	
	PhysicalAddress free_frame;
} static _mem_layout;


static Mmu040RootTableDescriptor *_root_td = (void *) MEM_MMU_TABLE_AREA;
//static Mmu040PointerTableDescriptor *pointer_td = (void *) (SDRAM_BASE + ROOT_LEVEL_DESCRIPTORS*sizeof(Mmu040RootTableDescriptor));

struct Mapper {
	Mmu040PageTableDescriptor *page_td;
	Mmu040PageTableDescriptor *page_descriptor;
	void *page[MAPPER_PAGES];
	int mapping;
} static _mapper = {
	.page_td = (void *) (MEM_MMU_TABLE_AREA + PAGE_SIZE),
	.page_descriptor = ((Mmu040PageTableDescriptor *) (MEM_MMU_TABLE_AREA + PAGE_SIZE)) + 2,
	//.page = (void *) (MEM_MMU_TABLE_AREA + PAGE_SIZE*2),
};

static void *_mapping_push(uint32_t physical_address) {
	int mapping = _mapper.mapping;
	Mmu040PageTableDescriptor desc = {
		.page = {
			.physical_address = PAGE_LEVEL_ADDR_FIELD(physical_address),
			.supervisor = true,
			.page_descriptor_type = MMU040_PAGE_DESCRIPTOR_TYPE_RESIDENT,
		},
	};
	
	if(mapping >= MAPPER_PAGES)
		return NULL;
	
	_mapper.page_descriptor[mapping] = desc;
	mmu040_invalidate_page(_mapper.page[mapping]);
	_mapper.mapping++;
	return _mapper.page[mapping];
}

static void _mapping_pop() {
	if(_mapper.mapping > 0)
		_mapper.mapping--;
}

static PhysicalAddress _alloc_frame() {
	PhysicalAddress free;
	PhysicalAddress *frame;
	
	if(!_mem_layout.free_frames)
		return (PhysicalAddress) NULL;
	
	_mem_layout.free_frames--;
	free = _mem_layout.free_frame;
	kprintf(LOG_LEVEL_DEBUG, "alloc frame 0x%X\n", free);
	
	frame = _mapping_push(free);
	
	_mem_layout.free_frame = *frame;
	mmu040_zero_4k(frame);
	
	_mapping_pop();
	
	//mmu_invalidate();
	return free;
}

static void _free_frame(PhysicalAddress frame) {
	PhysicalAddress *free;
	free = _mapping_push(frame);
	*free = _mem_layout.free_frame;
	_mem_layout.free_frame = frame;
	_mapping_pop();
}

static PhysicalAddress _build_free_frame_list() {
	//Mmu040RootTableDescriptor *root_table;
	Mmu040PointerTableDescriptor *pointer_table;
	Mmu040PageTableDescriptor *page_table;
	
	uint32_t i, j, k;
	PhysicalAddress ph;
	
	PhysicalAddress free_frame = (PhysicalAddress) MMU_DRAM_START;
	
	for(i = 0; i < ROOT_LEVEL_DESCRIPTORS; i++) {
		if(!UDT_IS_RESIDENT(_root_td[i].table.upper_level_descriptor_type))
			continue;
		
		ph = ROOT_LEVEL_FIELD_ADDR(_root_td[i].table.table_address);
		pointer_table = (void *) REPAGE(ph, _mapping_push(ph));
		kprintf(LOG_LEVEL_DEBUG, "[%u] pointer table 0x%X -> 0x%X (0x%X)\n", i, (uint32_t) pointer_table, mmu040_test_read(pointer_table), ROOT_LEVEL_FIELD_ADDR(_root_td[i].table.table_address));
		
		for(j = 0; j < POINTER_LEVEL_DESCRIPTORS; j++) {
			if(!UDT_IS_RESIDENT(pointer_table[j].table.upper_level_descriptor_type))
				continue;
			
			ph = POINTER_LEVEL_FIELD_ADDR(pointer_table[j].table.table_address);
			page_table = (void *) REPAGE(ph, _mapping_push(ph));
			kprintf(LOG_LEVEL_DEBUG, "    [%u] page table 0x%X -> 0x%X (0x%X)\n", j, (uint32_t) page_table, mmu040_test_read(page_table), POINTER_LEVEL_FIELD_ADDR(pointer_table[j].table.table_address));
			
			
			for(k = 0; k < PAGE_DESCRIPTORS; k++) {
				if(!PDT_IS_RESIDENT(page_table[k].page.page_descriptor_type))
					continue;
				
				ph = PAGE_LEVEL_FIELD_ADDR(page_table[k].page.physical_address);
				kprintf(LOG_LEVEL_DEBUG, "        [%u] page 0x%X\n", k, (uint32_t) ph);
				
				if(ph >= free_frame) {
					free_frame = ph + MMU_PAGE_SIZE;
					_mem_layout.free_frames--;
				}
			}
			
			_mapping_pop();
		}
		
		_mapping_pop();
	}
	
	PhysicalAddress free, *frame;
	for(free = free_frame; free < (MMU_DRAM_START + _mem_layout.total_frames*MMU_PAGE_SIZE); free += MMU_PAGE_SIZE) {
		frame = _mapping_push(free);
		*frame = _mem_layout.free_frame;
		_mem_layout.free_frame = free;
		_mapping_pop();
	}
	
	return _mem_layout.free_frame;
}

static void _get_table_indices(void *virtual_address, uint32_t *root_table_index, uint32_t *pointer_table_index, uint32_t *page_table_index) {
	uint32_t virtual = (uint32_t) virtual_address;
	*root_table_index = (virtual >> (32 - ROOT_LEVEL_DESCRIPTOR_BITS)) & (ROOT_LEVEL_DESCRIPTORS - 1);
	*pointer_table_index = (virtual >> (32 - ROOT_LEVEL_DESCRIPTOR_BITS - POINTER_LEVEL_DESCRIPTOR_BITS)) & (POINTER_LEVEL_DESCRIPTORS - 1);
	*page_table_index = (virtual >> PAGE_OFFSET_BITS) & (PAGE_LEVEL_DESCRIPTORS - 1);
}

//static Mmu040PointerTableDescriptor *_allocate_pointer_level_table() {
	//pointer_td += POINTER_LEVEL_DESCRIPTORS;
	
	//if(!(((uint32_t) pointer_td) & (POINTER_LEVEL_DESCRIPTORS - 1))) {
		//pointer_td = _alloc_frame();
	//}
	
	//return pointer_td;
//}

//static Mmu040PageTableDescriptor *_allocate_page_table() {
	//page_td += PAGE_DESCRIPTORS;
	
	//if(!(((uint32_t) page_td) & (PAGE_DESCRIPTORS - 1))) {
		//page_td = _alloc_frame();
	//}
	
	//return page_td;
//}

void mmu040_init() {
	int i;
	for(i = 0; i < MAPPER_PAGES; i++) {
		_mapper.page[i] = (void *) (MEM_MMU_TABLE_AREA + PAGE_SIZE*2 + PAGE_SIZE*i);
	}
	
	_mem_layout.total_frames = (64UL*1024UL*1024UL) / MMU_PAGE_SIZE;
	_mem_layout.free_frames = _mem_layout.total_frames;
	_mem_layout.free_frame = (PhysicalAddress) NULL;
	
	_build_free_frame_list();
}

//void *mmu040_get_physical_manual(uint32_t virtual_address) {
	//uint32_t root_table_index, pointer_table_index, page_table_index;
	//Mmu040PointerTableDescriptor *pointer_table;
	//Mmu040PageTableDescriptor *page_table;
	//void *frame;
	
	//_get_table_indices(virtual_address, &root_table_index, &pointer_table_index, &page_table_index);
	
	//if(UDT_IS_RESIDENT(root_td[root_table_index].table.upper_level_descriptor_type)) {
		//pointer_table = (void *) (root_td[root_table_index].table.table_address << ROOT_LEVEL_DESCRIPTOR_BITS);
	//} else {
		//printf("Failed root table lookup\n");
		//return NULL;
	//}
	
	//if(UDT_IS_RESIDENT(pointer_table[pointer_table_index].table.upper_level_descriptor_type)) {
		//page_table = (void *) (pointer_table[pointer_table_index].table.table_address << POINTER_LEVEL_DESCRIPTOR_BITS);
	//} else {
		//printf("Failed pointer table lookup\n");
		//return NULL;
	//}
	
	//if(PDT_IS_RESIDENT(page_table[page_table_index].page.page_descriptor_type)) {
		//frame = (void *) (page_table[page_table_index].page.physical_address << PAGE_OFFSET_BITS);
	//} else {
		//printf("Failed page table lookup\n");
		//return NULL;
	//}
	//return frame;
//}


int mmu040_init_userspace(MmuUserspaceHandle *userspace) {
	PhysicalAddress directory;
	Mmu040RegRootPointer *urp;
	
	if(!userspace)
		return -EINVAL;
	
	urp = (void *) userspace;
	
	memset(urp, 0, sizeof(Mmu040RegRootPointer));
	//kprintf(LOG_LEVEL_INFO, "Setting up empty userspace\n");
	if(!(directory = _alloc_frame()))
		return -ENOMEM;
	//memset(dir, 0, MMU_PAGE_SIZE);
	
	urp->root_pointer = directory >> SRP_URP_DESCRIPTOR_BITS;
	mmu040_set_urp(urp);
	
	return 0;
}

void mmu040_free_userspace(MmuUserspaceHandle *userspace) {
	//MmuDescriptorShort *dir;
	//MmuDescriptorShort *page;
	//int i, j;
	
	//if(!crp || crp->descriptor_type != MMU_DESCRIPTOR_TYPE_TABLE_SHORT)
		//return;
	
	//dir = (void *) (crp->table_address << 4);
	//for(i = 0; i < 1024; i++) {
		//if(dir[i].table.descriptor_type != MMU_DESCRIPTOR_TYPE_TABLE_SHORT)
			//continue;
		
		//page = (void *) (dir[i].table.table_address << 4);
		//for(j = 0; j < 1024; j++) {
			//if(page[i].page.descriptor_type != MMU_DESCRIPTOR_TYPE_PAGE)
				//continue;
			
			////TODO: refcount?
			//mmu_free_frame((void *) (page[i].page.page_address << 8));
		//}
	//}
}

int mmu040_clone_userspace(MmuUserspaceHandle *from, MmuUserspaceHandle *to) {
	/*MmuDescriptorShort *from_dir, *to_dir;
	MmuDescriptorShort *from_page, *to_page;
	void *p;
	int i, j;
	
	if(!(from && to))
		return -EINVAL;
	
	if(from->descriptor_type != MMU_DESCRIPTOR_TYPE_TABLE_SHORT || to->descriptor_type != MMU_DESCRIPTOR_TYPE_TABLE_SHORT)
		return -EFAULT;
	
	from_dir = (void *) (from->table_address << 4);
	to_dir = (void *) (to->table_address << 4);
	for(i = 0; i < 1024; i++) {
		if(from_dir[i].table.descriptor_type != MMU_DESCRIPTOR_TYPE_TABLE_SHORT) {
			to_dir[i].table.descriptor_type = MMU_DESCRIPTOR_TYPE_INVALID;
			continue;
		}
		
		from_page = (void *) (from_dir[i].table.table_address << 4);
		kprintf(LOG_LEVEL_DEBUG, "clone: table at 0x%X\n", MMU_PAGE_SIZE*1024*i);
		if(!(to_page = mmu_alloc_frame()))
			return -ENOMEM;
		to_dir[i].table.descriptor_type = MMU_DESCRIPTOR_TYPE_TABLE_SHORT;
		to_dir[i].table.used = false;
		to_dir[i].table.write_protected = false;
		to_dir[i].table.table_address = ((uint32_t) to_page) >> 4;
		
		for(j = 0; j < 1024; j++) {
			if(from_page[j].page.descriptor_type != MMU_DESCRIPTOR_TYPE_PAGE) {
				to_page[j].page.descriptor_type = MMU_DESCRIPTOR_TYPE_INVALID;
				continue;
			}
			
			//TODO: refcount, share mappings
			if(!(p = mmu_alloc_frame()))
				return -ENOMEM;
			memcpy(p, (void *) (from_page[j].page.page_address << 8), MMU_PAGE_SIZE);
			
			to_page[j].page.descriptor_type = MMU_DESCRIPTOR_TYPE_PAGE;
			to_page[j].page.cache_inhibit = false;			
			to_page[j].page.modified = false;			
			to_page[j].page.used = false;			
			to_page[j].page.write_protected = from_page[j].page.write_protected;
			to_page[j].page.page_address = ((uint32_t) p) >> 8;
		}
	}*/
	return 0;
}

int mmu040_switch_userspace(MmuUserspaceHandle *userspace) {
	if(!userspace)
		return -EINVAL;
	
	mmu040_set_urp((void *) userspace);
	
	return 0;
}

PhysicalAddress mmu_alloc_at(void *virt, bool supervisor, bool write_protected) {
	Mmu040RegRootPointer rp;
	uint32_t root_table_index, pointer_table_index, page_table_index;
	Mmu040RootTableDescriptor *root_table;
	Mmu040PointerTableDescriptor *pointer_table;
	Mmu040PageTableDescriptor *page_table;
	PhysicalAddress frame, ph;
	
	if(supervisor)
		mmu040_get_srp(&rp);
	else
		mmu040_get_urp(&rp);
	
	_get_table_indices(virt, &root_table_index, &pointer_table_index, &page_table_index);
	
	ph = rp.root_pointer << SRP_URP_DESCRIPTOR_BITS;
	root_table = REPAGE(ph, _mapping_push(ph));;
	
	if(UDT_IS_RESIDENT(root_table[root_table_index].table.upper_level_descriptor_type)) {
		ph = ROOT_LEVEL_FIELD_ADDR(root_table[root_table_index].table.table_address);
		pointer_table = (void *) REPAGE(ph, _mapping_push(ph));
	} else {
		ph = _alloc_frame();
		pointer_table = _mapping_push(ph);
		
		root_table[root_table_index].table.table_address = ROOT_LEVEL_ADDR_FIELD(ph);
		root_table[root_table_index].table.upper_level_descriptor_type = MMU040_UPPER_LEVEL_DESCRIPTOR_TYPE_RESIDENT;
	}
	
	if(UDT_IS_RESIDENT(pointer_table[pointer_table_index].table.upper_level_descriptor_type)) {
		ph = POINTER_LEVEL_FIELD_ADDR(pointer_table[pointer_table_index].table.table_address);
		page_table = (void *) REPAGE(ph, _mapping_push(ph));
	} else {
		ph = _alloc_frame();
		page_table = _mapping_push(ph);
		
		pointer_table[pointer_table_index].table.table_address = POINTER_LEVEL_ADDR_FIELD(ph);
		pointer_table[pointer_table_index].table.upper_level_descriptor_type = MMU040_UPPER_LEVEL_DESCRIPTOR_TYPE_RESIDENT;
	}
	
	if(PDT_IS_RESIDENT(page_table[page_table_index].page.page_descriptor_type)) {
		page_table[page_table_index].page.write_protected = write_protected;
		_free_frame(PAGE_LEVEL_FIELD_ADDR(page_table[page_table_index].page.physical_address));
	}
	
	frame = _alloc_frame();
	
	page_table[page_table_index].page.physical_address = PAGE_LEVEL_ADDR_FIELD(frame);
	page_table[page_table_index].page.supervisor = supervisor;
	page_table[page_table_index].page.cache_mode = MMU040_CACHE_MODE_CACHE_COPY_BACK;
	page_table[page_table_index].page.write_protected = write_protected;
	page_table[page_table_index].page.page_descriptor_type = MMU040_PAGE_DESCRIPTOR_TYPE_RESIDENT;
	
	_mapping_pop();
	_mapping_pop();
	_mapping_pop();
	
	return frame;
}

void mmu_free_at(void *virt, bool supervisor) {
	//TODO: implement
}

void mmu040_fill_frame(PhysicalAddress frame, int offset, void *src, unsigned int size) {
	uint8_t *map;
	uint8_t *src_byte = src;
	int i;
	
	map = _mapping_push(frame);
	
	for(i = 0; i < size; i++) {
		map[i + offset] = src_byte[i];
	}
	
	_mapping_pop();
}

void mmu_print_status() {
	kprintf(LOG_LEVEL_INFO, "%lu kB of %lu kB RAM used\n", (_mem_layout.total_frames - _mem_layout.free_frames)*(MMU_PAGE_SIZE/1024), _mem_layout.total_frames*(MMU_PAGE_SIZE/1024));
}
