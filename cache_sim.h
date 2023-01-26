#ifndef ONLINE_CACHE_SIMULATOR_CACHE_SIM_H_
#define ONLINE_CACHE_SIMULATOR_CACHE_SIM_H_

#include <sys/resource.h>  //for rusage

#include <iomanip>   // std::setw
#include <iostream>  // std::ostream, std::endl
#include <vector>    // vector

#include "absl/time/clock.h"

#ifdef DEBUG_PERF
inline uint8_t _depth = 0;
#define STARTTIME(X)    \
  auto X = absl::Now(); \
  _depth++;
#define STOPTIME(X)                         \
  _depth--;                                 \
  for (uint8_t _i = 0; _i < _depth; _i++) { \
    std::cout << "\t";                      \
  }                                         \
  std::cout << #X ": " << absl::Now() - X << std::endl;
#else  // DEBUG_PERF
#define STARTTIME(X)
#define STOPTIME(X)
#endif  // DEBUG_PERF

// number of bits needed to specify number of requests
#ifdef ADDR_BIT32
using req_count_t = uint32_t;
#else
using req_count_t = uint64_t;
#endif

// symbolizes a page that is a miss on every cache size
constexpr req_count_t infinity = 0; // every hit must have stack distance of at least 1

static inline double get_max_mem_used() {
  struct rusage data;
  getrusage(RUSAGE_SELF, &data);
  return (double)data.ru_maxrss / 1024.0;
}

static inline double percent(double val, double total) {
  return round((val / total) * 1000000) / 10000;
}

class CacheSim {
 protected:
  uint64_t access_number = 1;  // simulated timestamp and number of total requests
  size_t memory_usage = 0;     // memory usage of the cache sim

#ifdef ALL_METRICS
  std::vector<req_count_t> distance_vector;
#endif  // ALL_METRICS

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

  void dump_success_function(std::ostream& os, SuccessVector succ, size_t sample_rate = 1) {
    assert(sample_rate < succ.size());
    size_t total_requests = access_number - 1;
    os << "#" << std::setw(15) << "Cache Size" << std::setw(16) << "Total Hits" << std::setw(16)
       << "Hit Rate" << std::endl;
    for (size_t page = 1; page < succ.size(); page += sample_rate) {
      os << std::setw(16) << page << std::setw(16) << succ[page] << std::setw(16)
         << percent(succ[page], total_requests) << "%" << std::endl;
    }

    // Finally dump the number of forced misses
    size_t misses = total_requests - succ[succ.size() - 1];
    os << std::setw(16) << "Misses" << std::setw(16) << misses << std::setw(16)
       << percent(misses, total_requests) << "%" << std::endl;
  }
#ifdef ALL_METRICS
  void dump_all_metrics(std::ostream& succ_stream, std::ostream& vec_stream, SuccessVector succ) {
    // dump histogram and success function
    size_t total_requests = access_number;
    size_t prev = 0;
    succ_stream << "#" << std::setw(15) << "Cache Size" << std::setw(16) << "Adtl. Hits"
                << std::setw(16) << "Total Hits" << std::setw(16) << "Hit Rate" << std::endl;
    for (size_t page = 1; page < succ.size(); page++) {
      req_count_t cur = succ[page] - prev;
      prev = succ[page];
      succ_stream << std::setw(16) << page << std::setw(16) << cur << std::setw(16) << succ[page]
                  << std::setw(16) << percent(succ[page], total_requests) << "%" << std::endl;
    }

    // Dump the number of forced misses to succ_stream
    size_t misses = total_requests - succ[succ.size() - 1];
    succ_stream << std::setw(16) << "Misses" << std::setw(16) << misses << std::setw(16) << misses
                << std::setw(16) << percent(misses, total_requests) << "%" << std::endl;

    // dump the distance vector to vec_stream
    vec_stream << "#" << std::setw(15) << "Req Index" << std::setw(16) << "Stack Depth" << std::endl;
    for (size_t req = 0; req < distance_vector.size(); req++) {
      if (distance_vector[req] != infinity)
        vec_stream << std::setw(16) << req << std::setw(16) << distance_vector[req] << std::endl;
      else
        vec_stream << std::setw(16) << req << std::setw(16) << "infinity" << std::endl;
    }
  }
#endif  // ALL_METRICS
};

#endif  // ONLINE_CACHE_SIMULATOR_INCLUDE_CACHE_SIM_H_
