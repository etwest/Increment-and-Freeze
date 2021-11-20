#include "../include/RAM.h"

LRU_RAM::LRU_RAM(uint64_t size, uint32_t page): RAM(size, page) {
	uint32_t num_pages = memory_size / page_size;

	for (int i = 0; i < num_pages; i++) {
		Page *p = new Page(i * page_size);

		free_pages.push_back(p);
		memory.push_back(p);
	}

	// printf("Created LRU memory with %u pages\n", num_pages);
}

void LRU_RAM::memory_access(uint64_t virtual_addr) {
	uint64_t ts = access_number++;

	if (page_table.count(virtual_addr) > 0 && page_table[virtual_addr]->get_virt() == virtual_addr) {
		page_table[virtual_addr]->access_page(ts); // this is a memory access to the page stored in memory (it should update timestamp)
		moveFrontQueue(mapToQueue[virtual_addr], ts); // move this page to the front of the LRU queue
	} else {
		// if there are free pages then use one of them
		Page *p;
		if (free_pages.size() > 0) {
			p = free_pages.front();
			free_pages.pop_front();
		} else {
			p = evict_oldest();
		}
		p->place_page(virtual_addr, ts);
		page_table[p->get_virt()] = p;
		LRU_queue.insert(ts, p->get_virt());
        mapToQueue[virtual_addr] = ts;
		page_faults++;
	}
}

Page *LRU_RAM::evict_oldest() {
	//unmap virtual address
	uint64_t virt = LRU_queue.getLast();
    mapToQueue.erase(virt);
	LRU_queue.remove(LRU_queue.getWeight()-1);
    return page_table[virt];
}

void LRU_RAM::moveFrontQueue(uint64_t curts, uint64_t newts) {
	std::pair<size_t, uint64_t> found = LRU_queue.find(curts);

    LRU_queue.remove(found.first);
    LRU_queue.insert(newts, found.second);

    mapToQueue[found.second] = newts;
}
