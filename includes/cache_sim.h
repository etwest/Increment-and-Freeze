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
#include <chrono>


#ifdef DEBUG_PERF
inline uint8_t _depth = 0;
#define STARTTIME(X) auto X = std::chrono::high_resolution_clock::now(); _depth++;
#define STOPTIME(X)  \
    _depth--; \
    for (uint8_t _i = 0; _i < _depth; _i++) {std::cout << "\t";} \
    auto Y = std::chrono::high_resolution_clock::now(); \
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(Y - X); \
    std::cout << #X ": " << dur.count() << "us" << std::endl;
#else //DEBUG_PERF
#define STARTTIME(X) 
#define STOPTIME(X)  
#endif //DEBUG_PERF

// number of bits needed to specify number of requests
#ifdef ADDR_BIT32
typedef uint32_t req_count_t;
typedef int32_t sign_req_count_t;
constexpr size_t mask_bits = 31;
#else
typedef uint64_t req_count_t;
typedef int64_t sign_req_count_t;
constexpr size_t mask_bits = 63;
#endif

// ifs to use if some statements are likely to be true or false.
#define likely_if(x) if(__builtin_expect((bool)(x), true))
#define unlikely_if(x) if (__builtin_expect((bool)(x), false))

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
  uint64_t sample_access_number = 1; // simulated timestamp and number of total requests 
  size_t memory_usage = 0;    // memory usage of the cache sim
  // Controls how many unique addresses are sampled by IAF to construct the hit-rate curve
  // On average, every 1 in 2^sample_rate addresses will be sampled.
  // by default this value is 0 (no sampling)
  const size_t sample_mask;

  // seed for hash function used in address sampling
  const size_t sample_seed;
  
  // determine which partition is used
  const size_t sample_partition;

 public:
  using SuccessVector = std::vector<req_count_t>;

  CacheSim() = delete;
  virtual ~CacheSim() = default;
  CacheSim(size_t _sample_mask, size_t _sample_seed, size_t _sample_partition)
  : sample_mask(_sample_mask), sample_seed(_sample_seed), sample_partition(_sample_partition)
   {};
  /*
   * Perform a memory access upon a given id
   * addr:    the id to access 
   * returns  nothing
   */
  virtual bool should_sample(req_count_t addr) = 0;
  virtual bool memory_access(req_count_t addr, req_count_t nblocks = 1) = 0;

  virtual SuccessVector get_success_function() = 0;
  
  double get_memory_usage() { return get_max_mem_used(); }
  
  void inc_access(uint64_t count) {access_number += count;};

  void dump_success_function(std::ostream& os, SuccessVector succ, size_t stride=1) {
    assert(stride < succ.size());
    size_t total_requests = access_number - 1;
    if (sample_mask)
      total_requests = (sample_access_number-1) * (sample_mask+1);
    os << "#" << std::setw(15) << "Cache Size" << std::setw(16) 
       << "Hits" << std::setw(16) << "Hit Rate" << std::endl;
    for (size_t page = 1; page < succ.size(); page+=stride) {
      os << std::setw(16) << page << std::setw(16) << succ[page]
         << std::setw(16) << percent(succ[page], total_requests) << "%" << std::endl;
    }

    // Finally dump the number of forced misses
    size_t misses = total_requests - (succ.size() ? succ[succ.size() - 1] : 0);
    os << std::setw(16) <<"Misses" << std::setw(16) << misses 
       << std::setw(16) << percent(misses, total_requests) << "%" << std::endl;
  }
  
  void csv_success_function(std::ostream& os, SuccessVector succ, size_t stride=1) {
    if (succ.size() == 0)
      return;
    assert(stride < succ.size());
    //FIXME do for dump too
    size_t total_requests = access_number - 1;
    if (sample_mask)
      total_requests = (sample_access_number-1) * (sample_mask+1);
    // Print the number of requests and largest cache size (and number of reqs, including filtered out)
    os << total_requests << "," << succ.size() - 1 << "," << access_number-1 << std::endl;  
    os << "Cache Size,Hits" << std::endl;
    for (size_t page = 1; page < succ.size(); page+=stride) {
      os << page << "," << succ[page] << std::endl;
    }
  }

  void print_small_csv(std::ostream& os, SuccessVector succ) {
    if (succ.size() <= 1)
      return;

    size_t total_requests = access_number - 1;
    if (sample_mask)
      total_requests = (sample_access_number-1) * (sample_mask+1);

    os << total_requests << "," << succ.size() - 1 << "," << access_number-1 << std::endl;
    os << "Cache Size,Hits" << std::endl;

    // Always print the first point.
    os << 1 << "," << succ[1] << std::endl;

    double last_printed_hits = succ[1];
    size_t last_printed_page = 1;

    for (size_t i = 2; i < succ.size(); ++i) {
        // Print if we have < 1000 total cache sizes, or if cache size grew by 5%, or hits grew by 1%
        if (succ.size() < 1000 || i >= last_printed_page * 1.05 || succ[i] >= last_printed_hits * 1.01) {
            os << i << "," << succ[i] << std::endl;
            last_printed_hits = succ[i];
            last_printed_page = i;
        }
    }

    // Always print the last point if it hasn't been printed.
    if (succ.size() - 1 > last_printed_page) {
        os << succ.size() - 1 << "," << succ.back() << std::endl;
    }
  }
};

#endif  // ONLINE_CACHE_SIMULATOR_INCLUDE_CACHE_SIM_H_
