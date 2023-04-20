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

#ifndef ONLINE_CACHE_SIMULATOR_INCLUDE_IAKWRAPPER_H_
#define ONLINE_CACHE_SIMULATOR_INCLUDE_IAKWRAPPER_H_

#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

#include "cache_sim.h"
#include "increment_and_freeze.h"

class BoundedIAF : public CacheSim {
  private:
    using ChunkInput = IncrementAndFreeze::ChunkInput;
    using ChunkOutput = IncrementAndFreeze::ChunkOutput;
    // Struct that holds hits vector, living requests, and chunk requests to process
    ChunkInput chunk_input;

    IncrementAndFreeze iaf_alg;

    size_t cur_u;
    size_t max_living_req;

    constexpr static size_t max_u_mult = 4; // chunk <= max_u_mult * u
    constexpr static size_t min_u_mult = 3; // chunk > min_u_mult * u

    // Function to update value of u given a living requests size
    inline void update_u(size_t num_living) {
      size_t upper_u = max_u_mult * num_living;
      cur_u = num_living * min_u_mult < cur_u ? cur_u : upper_u;
    };

    void process_requests();

  public:
    // Logs a memory access to simulate. The order this function is called in matters.
    void memory_access(req_count_t addr);
    
    /* Returns the success function after processing requests in the current chunk.
     * Does some work, up to u log u depending on the number of unprocessed requests.
     */
    SuccessVector get_success_function();

    inline size_t get_u() { return cur_u; };
    inline size_t get_mem_limit() { return max_living_req; };

    // BoundedIAF Constructor.
    // min_chunk_size: minimum size of requests array before running IAK bigger values of 
    //                 min_chunk_size means more parallelism but more memory consumption.
    // max_cache_size: Limit on the memory sizes for which we report the hit rate. For example a 
    //                 max cache size of 1 GiB means that we report hit rate for all memory sizes
    //                 <= 1 GiB.
    BoundedIAF(size_t min_chunk_size=65536, size_t max_cache_size=((size_t)-1)/max_u_mult) 
      : cur_u(min_chunk_size), max_living_req(max_cache_size) {};
    ~BoundedIAF() = default;
};

#endif  // ONLINE_CACHE_SIMULATOR_INCLUDE_BOUNDED_IAF_H_
