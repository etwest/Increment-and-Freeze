#include "../include/RAM.h"
#include <iostream>

LruSizesSim::LruSizesSim(uint64_t size, uint32_t page): RAM(size, page) {
    num_pages = memory_size / page_size;

    for (int i = 0; i < num_pages; i++) {
        Page *p = new Page(i * page_size);

        free_pages.push_back(p);
        memory.push_back(p);
    }
    page_faults.resize(num_pages+1);

    // printf("Created LRU memory with %u pages\n", num_pages);
}
LruSizesSim::~LruSizesSim() {
    for (int i = 0; i < num_pages; i++) {
        delete memory[i];
    }
}

void LruSizesSim::memory_access(uint64_t virtual_addr) {
    uint64_t ts = access_number++;

    if (page_table.count(virtual_addr) > 0 && page_table[virtual_addr]->get_virt() == virtual_addr) {
    	// Page in memory so move it to the front of the LRU queue
        page_faults[move_front_queue(page_table[virtual_addr]->last_touched(), ts)]++;
        page_table[virtual_addr]->access_page(ts); // update timestamp of the page
    } 
    else {
        // if there are free pages then use one of them
        page_faults[num_pages]++;
        Page *p;
        if (free_pages.size() > 0) {
            p = free_pages.front();
            free_pages.pop_front();
        } 
        else { // no free pages so evict the oldest page
            p = evict_oldest();
            //page_table.erase(p->get_virt());
        }
        p->place_page(virtual_addr, ts);
        page_table[p->get_virt()] = p;
        LRU_queue.insert(ts, p->get_virt());
        assert(p->get_virt() ==  virtual_addr);
        //page_faults++;
    }
}

Page *LruSizesSim::evict_oldest() {
    //unmap virtual address
    uint64_t virt = LRU_queue.get_last();
    LRU_queue.remove(LRU_queue.get_weight()-1);
    return page_table[virt];
}

size_t LruSizesSim::move_front_queue(uint64_t oldts, uint64_t newts) {
    std::pair<size_t, uint64_t> found = LRU_queue.find(oldts);

    LRU_queue.remove(found.first);
    LRU_queue.insert(newts, found.second);
    return found.first;
}

std::vector<uint64_t> LruSizesSim::get_success_function() {
    uint64_t nfaults = 0;
    std::vector<uint64_t> faults(num_pages+1);
    for (uint32_t page = num_pages; page > 0; page--) {
        nfaults += page_faults[page];
        faults[page] = nfaults;
    }
    //faults[0] = nfaults + page_faults[0];
    return faults;
}

void LruSizesSim::print_success_function() {
    std::vector<uint64_t> faults = get_success_function();
    for (uint32_t page = 1; page <= num_pages; page++) {
        std::cout << page << ": " << faults[page] << std::endl;
    }
}
