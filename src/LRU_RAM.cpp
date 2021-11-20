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
		moveFrontQueue(mapToQueue[virtual_addr]); // move this page to the front of the LRU queue
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
		LRU_queue.push_front(p);
        mapToQueue[virtual_addr] = LRU_queue.begin();
		page_faults++;
	}
}

Page *LRU_RAM::evict_oldest() {
	Page *old_page = LRU_queue.back();

    LRU_queue.pop_back();
    mapToQueue.erase(old_page->get_virt());
    return old_page;
}

void LRU_RAM::moveFrontQueue(std::list<Page *>::iterator queue_elm) {
    Page *p = *queue_elm;

    LRU_queue.erase(queue_elm);
    LRU_queue.push_front(p);

    mapToQueue[p->get_virt()] = LRU_queue.begin();
}
