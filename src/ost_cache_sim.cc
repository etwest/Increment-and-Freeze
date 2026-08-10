/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel DeLayo, Bradley Kuszmaul, Evan West
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

#include "ost_cache_sim.h"

#include <utility>

constexpr uint64_t prime_64b = 13208052345836349601ull;

// perform a memory access and use the LRU_queue to update the success function
bool OSTCacheSim::memory_access(req_count_t addr, req_count_t nblocks) {
  uint64_t ts = access_number++;

  assert(nblocks == 1 && "OSTCache does not yet support nblocks!");

  // attempt to find the addr in the OSTree
  if (page_table.count(addr) > 0) {
    // this is not the first access to this page so lookup in the OSTree
    page_hits[move_front_queue(page_table[addr], ts)]++;
    page_table[addr] = ts;  // update the timestamp
    return true;
  }

  // new unique page increases the max memory (and is not a hit)
  page_hits.push_back(0);
  page_table[addr] = ts;  // new PTE

  // put the page in the LRU_queue
  LRU_queue.insert(ts, addr);
  return true;
}

// delete a page with a given timestamp from the LRU_queue and
// then insert it back with an updated timestamp.
// return the rank of the page before updating the timestamp
// assumes that a page with the old_ts exists in the LRU_queue
uint64_t OSTCacheSim::move_front_queue(uint64_t old_ts, uint64_t new_ts) {
  std::pair<uint64_t, req_count_t> found = LRU_queue.find(old_ts);

  LRU_queue.remove(found.first);
  LRU_queue.insert(new_ts, found.second);
  return found.first;
}

// return the success function by starting at the back of the
// page_hits vector and summing the elements to the front
CacheSim::SuccessVector OSTCacheSim::get_success_function() {
  req_count_t nhits = 0;

  // update the memory usage of the OSTreeSim
  memory_usage = LRU_queue.get_weight() * sizeof(OSTree);

  // build vector to return based upon the number of unique pages accessed
  // we index this vector by 1 so make it one larger
  CacheSim::SuccessVector success(page_hits.size()+1);
  for (req_count_t page = 0; page < page_hits.size(); page++) {
    nhits += page_hits[page];
    success[page+1] = nhits;  // faults at given size is sum of self and bigger
  }
  return success;
}

bool OSTCacheSim::should_sample(req_count_t addr) {
  if (sample_mask > 0) {
    // Universal Hashing from wikipedia
    __int128_t hash_big = (__int128_t)prime_64b * addr;// + rand_64b;

    uint64_t hash = hash_big >> 64;

    // ignore all requests whose hash value is incorrect
    // OLD VERSION
    // likely_if ((hash & sample_mask) != sample_partition) return false;
    return (hash & sample_mask) == sample_partition;
  }
  return true;
}
