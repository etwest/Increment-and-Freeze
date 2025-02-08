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

#ifndef ONLINE_CACHE_SIMULATOR_CACHE_SIM_H_
#define ONLINE_CACHE_SIMULATOR_CACHE_SIM_H_

#include <iostream>     // std::ostream, std::endl
#include <vector>       // vector
#include <iomanip>      // std::setw
#include <cmath>        // round
#include <cassert>      // assert

#include <sys/resource.h> //for rusage


#ifdef DEBUG_PERF
#include "absl/time/clock.h"
inline uint8_t _depth = 0;
#define STARTTIME(X) auto X = absl::Now(); _depth++;
#define STOPTIME(X)  \
    _depth--; \
    for (uint8_t _i = 0; _i < _depth; _i++) {std::cout << "\t";} \
    std::cout << #X ": " << absl::Now() - X << std::endl;
#else //DEBUG_PERF
#define STARTTIME(X) 
#define STOPTIME(X)  
#endif //DEBUG_PERF

// number of bits needed to specify number of requests
#ifdef ADDR_BIT32
typedef uint32_t req_count_t;
#else
typedef uint64_t req_count_t;
#endif

static inline double get_max_mem_used() {
  struct rusage data;
  getrusage(RUSAGE_SELF, &data);
  return (double) data.ru_maxrss / 1024.0;
}

static inline double percent(double val, double total) {
  return round((val / total) * 1000000) / 10000;
}

class CacheSim {
 protected:
  uint64_t access_number = 1; // simulated timestamp and number of total requests
  size_t memory_usage = 0;    // memory usage of the cache sim
 public:
  using SuccessVector = std::vector<req_count_t>;

  CacheSim() = default;
  virtual ~CacheSim() = default;
  /*
   * Perform a memory access upon a given id
   * addr:    the id to access 
   * returns  nothing
   */
  virtual void memory_access(req_count_t addr) = 0;

  virtual SuccessVector get_success_function() = 0;
  
  double get_memory_usage() { return get_max_mem_used(); }

  void dump_success_function(std::ostream& os, SuccessVector succ, size_t sample_rate=1) {
    assert(sample_rate < succ.size());
    size_t total_requests = access_number - 1;
    os << "#" << std::setw(15) << "Cache Size" << std::setw(16) 
       << "Hits" << std::setw(16) << "Hit Rate" << std::endl;
    for (size_t page = 1; page < succ.size(); page+=sample_rate) {
      os << std::setw(16) << page << std::setw(16) << succ[page]
         << std::setw(16) << percent(succ[page], total_requests) << "%" << std::endl;
    }

    // Finally dump the number of forced misses
    size_t misses = total_requests - (succ.size() ? succ[succ.size() - 1] : 0);
    os << std::setw(16) <<"Misses" << std::setw(16) << misses 
       << std::setw(16) << percent(misses, total_requests) << "%" << std::endl;
  }
};

#endif  // ONLINE_CACHE_SIMULATOR_INCLUDE_CACHE_SIM_H_
