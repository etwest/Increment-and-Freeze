/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel Delayo, Bradley Kuszmaul, Evan West
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "container_cache_sim.h"

#include <utility>

// perform a memory access and use the LRU_queue to update the success function
void ContainerCacheSim::memory_access(req_count_t addr) {
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
uint64_t ContainerCacheSim::move_front_queue(uint64_t old_ts, uint64_t new_ts) {
  auto it = LRU_queue.find(old_ts);

  uint64_t rank = it.rank();

  LRU_queue.erase(it);
  LRU_queue.insert(new_ts);
  return rank;
}

// return the success function by starting at the back of the
// page_hits vector and summing the elements to the front
CacheSim::SuccessVector ContainerCacheSim::get_success_function() {
  req_count_t nhits = 0;

  // update the memory usage of the OSTreeSim
  memory_usage = LRU_queue.size() * sizeof(cachelib::OrderStatisticSet<size_t, std::greater<>>::node_type);

  // build vector to return based upon the number of unique pages accessed
  // we index this vector by 1 so make it one larger
  CacheSim::SuccessVector success(page_hits.size()+1);
  for (req_count_t page = 0; page < page_hits.size(); page++) {
    nhits += page_hits[page];
    success[page+1] = nhits;  // faults at given size is sum of self and bigger
  }
  return success;
}
