#include "../include/RAM.h"
#include <iostream>

// construct the LRUSizesSim object
LruSizesSim::LruSizesSim(uint64_t size, uint32_t page): RAM(size, page) {
    for (int i = 0; i < num_pages; i++) {
        Page *p = new Page(i * page_size);

        free_pages.push_back(p);
        memory.push_back(p);
    }
    page_faults.resize(num_pages+1);

    // printf("Created LRU memory with %u pages\n", num_pages);
}

// free the pages in the memory
LruSizesSim::~LruSizesSim() {
    for (int i = 0; i < num_pages; i++) {
        delete memory[i];
    }
}

// perform a memory access and use the LRU_queue to update the success function
void LruSizesSim::memory_access(uint64_t virtual_addr) {
    uint64_t ts = access_number++;

    if (page_table.count(virtual_addr) > 0 && page_table[virtual_addr]->get_virt() == virtual_addr) {
    	// Page in memory so move it to the front of the LRU queue
        page_faults[move_front_queue(page_table[virtual_addr]->last_touched(), ts)]++;
        page_table[virtual_addr]->access_page(ts); // update timestamp of the page
        return;
    } 
    
    page_faults[num_pages]++; // a full sized memory still had a page fault
    Page *p;
    if (free_pages.size() > 0) {
        // if there are free pages then use one of them
        p = free_pages.front();
        free_pages.pop_front();
    } 
    else {
        // no free pages so evict the oldest page
        p = evict_oldest();
        page_table.erase(p->get_virt());
    }

    // map the virtual address to the page we found
    p->place_page(virtual_addr, ts);
    page_table[p->get_virt()] = p;
    assert(p->get_virt() ==  virtual_addr);

    // put the page in the LRU_queue
    LRU_queue.insert(ts, p->get_virt());
}

// identify the oldest page and remove it from the LRU_queue
Page *LruSizesSim::evict_oldest() {
    //unmap virtual address
    uint64_t virt = LRU_queue.get_last();
    LRU_queue.remove(LRU_queue.get_weight()-1);
    return page_table[virt];
}

// delete a page with a given timestamp from the LRU_queue and
// then insert it back with an updated timestamp.
// return the rank of the page before updating the timestamp
// assumes that a page with the old_ts exists in the LRU_queue
size_t LruSizesSim::move_front_queue(uint64_t old_ts, uint64_t new_ts) {
    std::pair<size_t, uint64_t> found = LRU_queue.find(old_ts);

    LRU_queue.remove(found.first);
    LRU_queue.insert(new_ts, found.second);
    return found.first;
}

// return the success function by starting at the back of the
// page_faults vector and summing the elements to the front
std::vector<uint64_t> LruSizesSim::get_success_function() {
    uint64_t nfaults = 0;
    std::vector<uint64_t> faults(num_pages+1); // the vector to return
    for (uint32_t page = num_pages; page > 0; page--) {
        nfaults += page_faults[page];
        faults[page] = nfaults; // faults at given size is sum of self and bigger
    }
    return faults;
}

// print out the success function
void LruSizesSim::print_success_function() {
    std::vector<uint64_t> faults = get_success_function();
    for (uint32_t page = 1; page <= num_pages; page++) {
        std::cout << page << ": " << faults[page] << std::endl;
    }
}
