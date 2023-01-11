#include "container_cache_sim.h"

#include <utility>

// perform a memory access and use the LRU_queue to update the success function
void ContainerCacheSim::memory_access(uint64_t addr) {
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
  LRU_queue.insert(ts);
}

// delete a page with a given timestamp from the LRU_queue and
// then insert it back with an updated timestamp.
// return the rank of the page before updating the timestamp
// assumes that a page with the old_ts exists in the LRU_queue
size_t ContainerCacheSim::move_front_queue(uint64_t old_ts, uint64_t new_ts) {
  auto it = LRU_queue.find(old_ts);

  size_t rank = it.rank();

  LRU_queue.erase(it);
  LRU_queue.insert(new_ts);
  return rank;
}

// return the success function by starting at the back of the
// page_hits vector and summing the elements to the front
CacheSim::SuccessVector ContainerCacheSim::get_success_function() {
  uint64_t nhits = 0;

  // update the memory usage of the OSTreeSim
  memory_usage = LRU_queue.size() * sizeof(cachelib::OrderStatisticSet<size_t, std::greater<>>::node_type);

  // build vector to return based upon the number of unique pages accessed
  // we index this vector by 1 so make it one larger
  CacheSim::SuccessVector success(page_hits.size()+1);
  for (uint64_t page = 0; page < page_hits.size(); page++) {
    nhits += page_hits[page];
    success[page+1] = nhits;  // faults at given size is sum of self and bigger
  }
  return success;
}
