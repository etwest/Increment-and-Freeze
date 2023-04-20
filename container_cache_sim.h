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

#ifndef ONLINE_CACHE_SIMULATOR_CONTAINER_CACHE_SIM_H_
#define ONLINE_CACHE_SIMULATOR_CONTAINER_CACHE_SIM_H_

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cache_sim.h"
#include "container/order_statistic_set.h"

/*
 * An ContainerCacheSim simulates LRU running on every possible
 * memory size from 1 to MEM_SIZE.
 * Returns a success function which gives the number of page faults
 * for every memory size.
 */
class ContainerCacheSim : public CacheSim {
 private:
  std::vector<req_count_t> page_hits;  // vector used to construct success function
  cachelib::OrderStatisticSet<uint64_t, std::greater<>> LRU_queue; // order statistics tree for LRU depth
  std::unordered_map<req_count_t, uint64_t> page_table;  // map from addr to ts
 public:
  ContainerCacheSim() = default;
  ~ContainerCacheSim() = default;

  /*
   * Performs a memory access upon a given virtual page
   * updates the page_hits vector based upon where the page
   * was found within the LRU_queue
   * virtual_addr:   the virtual address to access
   * returns         nothing
   */
  void memory_access(req_count_t addr);

  /*
   * Moves a page with a given timestamp to the front of the queue
   * and inserts the page back into the queue with an updated timestamp
   * old_ts:   the current timestamp of the page being accessed
   * new_ts:   the timestamp of the access
   * returns   the rank of the page with old_ts in the LRU_queue
   */
  uint64_t move_front_queue(uint64_t old_ts, uint64_t new_ts);

  /*
   * Get the success function. The success function gives the number of
   * page faults with every memory size from 1 to MEM_SIZE
   * returns   the success function in a vector
   */
  SuccessVector get_success_function();
};

#endif  // ONLINE_CACHE_SIMULATOR_CONTAINER_CACHE_SIM_H_
