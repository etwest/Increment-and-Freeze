#include "LruSizesSim.h"

#include <iostream>

// perform a memory access and use the LRU_queue to update the success function
void LruSizesSim::memory_access(uint64_t addr) {
  uint64_t ts = access_number++;

  // attempt to find the addr in the OSTree
  if (page_table.count(addr) > 0) {
    // this is not the first access to this page so lookup in the OSTree
    page_hits[move_front_queue(page_table[addr], ts)]++;
    page_table[addr] = ts;  // update the timestamp
    return;
  }

  // new unique page increases the max memory (and is not a hit)
  page_hits.push_back(0);
  page_table[addr] = ts;  // new PTE

  // put the page in the LRU_queue
  LRU_queue.insert(ts, addr);
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
// page_hits vector and summing the elements to the front
std::vector<uint64_t> LruSizesSim::get_success_function() {
  uint64_t nhits = 0;

  // build vector to return based upon the number of unique pages accessed
  // we index this vector by 1 so make it one larger
  std::vector<uint64_t> success(page_hits.size()+1);
  for (uint32_t page = 0; page < page_hits.size(); page++) {
    nhits += page_hits[page];
    success_func[page+1] = nhits;  // faults at given size is sum of self and bigger
  }
  return success_func;
}
