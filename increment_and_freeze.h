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

#ifndef ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
#define ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_

#include <cassert>      // for assert
#include <cstddef>      // for size_t
#include <cstdint>      // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>     // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>      // for pair, move, swap
#include <vector>       // for vector, vector<>::iterator
#include <array>        // for array
#include <cmath>        // for ceil

#include "iaf_params.h" // for kIafBranching
#include "cache_sim.h"  // for CacheSim
#include "op.h"         // for op
#include "partition.h"  // for partitionstate
#include "projection.h" // for ProjSequence

// Implements the IncrementAndFreezeInPlace algorithm
class IncrementAndFreeze: public CacheSim {
 public:
  struct request {
    req_count_t addr;
    req_count_t access_number;

    inline bool operator< (request oth) const {
      return addr < oth.addr || (addr == oth.addr && access_number < oth.access_number);
    }

    request() = default;
    request(req_count_t a, req_count_t n) : addr(a), access_number(n) {}
  };
  static_assert(sizeof(request) == 2*sizeof(req_count_t));

  struct ChunkOutput {
    std::vector<request> living_requests;
    std::vector<req_count_t> hits_vector;
  };

  struct ChunkInput {
    ChunkOutput output;            // where to place chunk result
    std::vector<request> requests; // living requests and fresh requests
  };
 private:
  // A vector of all requests
  std::vector<request> requests;

  // Vector of operations used in ProjSequence to store memory operations
  std::vector<Op> operations;

  /* This converts the requests into the previous and next vectors
   * Requests is copied, not modified.
   * Precondition: requests must be properly populated.
   * Returns: number of unique ids in requests
   */
  req_count_t populate_operations(std::vector<request> &req, std::vector<request> *living_req);

  /* Helper function for update_hits_vector
   * Recursively (and in parallel) populates the distance vector if the
   * projection is small enough, or calls itself with smaller projections otherwise.
   */
  void do_projections(std::vector<req_count_t>& distance_vector, ProjSequence seq);
 
  /*
   * Helper function for solving a projected sequence using the brute force algorithm
   * This takes time O(n^2) but requires no recursion or other overheads. Thus, we can
   * use it to solve larger ProjSequences.
   */
  void do_base_case(std::vector<req_count_t>& distance_vector, ProjSequence seq);

  /*
   * Update a hits vector with the stack depths of the memory requests found in reqs
   * reqs:        vector of memory requests to update the hits vector with
   * hits_vector: A hits vector indicates the number of requests that required a given memory amount
   */
  void update_hits_vector(std::vector<request>& reqs, std::vector<req_count_t>& hits_vector,
                          std::vector<request> *living_req=nullptr);
 public:
  // Logs a memory access to simulate. The order this function is called in matters.
  void memory_access(req_count_t addr);
  /* Returns the success function.
   * Does *a lot* of work.
   * When calling print_success_function, the answer is re-computed.
   */
  SuccessVector get_success_function();

  /*
   * Process a chunk of requests (called by IAF_Wrapper)
   * Return the new living requests and the success function
   */
  void process_chunk(ChunkInput &input);

  IncrementAndFreeze() = default;
  ~IncrementAndFreeze() = default;
};

#endif  // ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
