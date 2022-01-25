#include "../include/RAM.h"
#include <iostream>

// construct the LRUSizesSim object
LruSizesSim::LruSizesSim() {
    // printf("Created LRU memory with %u pages\n", num_pages);
}

// free the pages in the memory
LruSizesSim::~LruSizesSim() {
}

// perform a memory access and use the LRU_queue to update the success function
void LruSizesSim::memory_access(uint64_t virtual_addr) {
    uint64_t ts = access_number++;
    page_hits.resize(access_number);

    if (page_table.count(virtual_addr) > 0 && page_table[virtual_addr]->get_virt() == virtual_addr) {
    	// Page in memory so move it to the front of the LRU queue
        page_hits[move_front_queue(page_table[virtual_addr]->last_touched(), ts)]++;
        page_table[virtual_addr]->access_page(ts); // update timestamp of the page
        return;
    } 
    
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
    //uint64_t virt = LRU_queue.get_last();
    uint64_t virt = LRU_queue.remove(LRU_queue.get_weight()-1);
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

// return the success function by starting at the front of the
// page_hits vector and summing the elements
std::vector<uint64_t> LruSizesSim::get_success_function() {
    uint64_t nhits = 0;
    std::vector<uint64_t> success(access_number); // the vector to return
    for (uint32_t page = 1; page < page_hits.size(); page++) {
        nhits += page_hits[page];
        success[page] = nhits; // faults at given size is sum of self and bigger
    }
    return success;
}
