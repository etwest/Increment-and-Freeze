#include "../include/RAM.h"

Clock_RAM::Clock_RAM(uint64_t size, uint32_t page): RAM(size, page) {
	uint32_t num_pages = memory_size / page_size;

	for (int i = 0; i < num_pages; i++) {
		Page *p = new Page(i * page_size);

		free_pages.push_back(p);
		memory.push_back(p);
	}

	// printf("Created Clock memory with %u pages\n", num_pages);
}

void Clock_RAM::memory_access(uint64_t virtual_addr) {
	uint64_t ts = access_number++;

	if (page_table.count(virtual_addr) > 0 && page_table[virtual_addr]->get_virt() == virtual_addr) {
		page_table[virtual_addr]->access_page(ts); // this is a memory access to the page stored in memory (clear ref)
	} else {
		// if there are free pages then use one of them
		Page *p;
		if (free_pages.size() > 0) {
			p = free_pages.front();
			free_pages.pop_front();
		} else {
			p = evict_clock();
		}
		p->place_page(virtual_addr, ts);
		page_table[p->get_virt()] = p;
		page_faults++;
	}
}

Page *Clock_RAM::evict_clock() {
	while(true) {
		if (!memory[hand]->get_R()) {
			Page *ret = memory[hand];
			inc_hand();
			return ret;
		}
		memory[hand]->clock_touch(); // this page has been touched by the clock algorithm
		inc_hand();
	}
}
