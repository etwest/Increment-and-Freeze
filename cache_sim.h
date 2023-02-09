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
constexpr req_count_t infinity = 0;  // every hit must have stack distance of at least 1

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
  std::vector<req_count_t> prev_vec;
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

  void dump_success_function(std::ostream& succ_stream, SuccessVector succ) {
    // dump histogram and success function
    size_t total_requests = access_number - 1;
    size_t prev = 0;
    succ_stream << "#" << std::setw(13) << "Cache Size" << std::setw(14) << "Adtl. Hits"
                << std::setw(14) << " Total Hits" << std::setw(14) << "Hit Rate" << "\n";
    for (size_t page = 1; page < succ.size(); page++) {
      req_count_t cur = succ[page] - prev;
      prev = succ[page];
      succ_stream << std::setw(14) << page << std::setw(14) << cur << std::setw(14)
                  << " " + std::to_string(succ[page]) << std::setw(14)
                  << percent(succ[page], total_requests) << "%" << "\n";
    }

    // Dump the number of forced misses to succ_stream
    size_t misses = total_requests - succ[succ.size() - 1];
    succ_stream << std::setw(14) << "Misses" << std::setw(14) << misses << std::setw(14)
                << " " + std::to_string(misses) << std::setw(14) << percent(misses, total_requests)
                << "%" << "\n";
  }
#ifdef ALL_METRICS
  void dump_all_metrics(std::ostream& succ_stream, std::ostream& vec_stream, SuccessVector succ) {
    const size_t bufsize = 256*1024;
    char buf[bufsize];
    vec_stream.rdbuf()->pubsetbuf(buf, bufsize);

    // dump histogram and success function
    size_t total_requests = access_number - 1;
    size_t prev = 0;
    succ_stream << "#" << std::setw(13) << "Cache Size" << std::setw(14) << "Adtl. Hits"
                << std::setw(14) << " Total Hits" << std::setw(14) << "Hit Rate" << "\n";
    for (size_t page = 1; page < succ.size(); page++) {
      req_count_t cur = succ[page] - prev;
      prev = succ[page];
      succ_stream << std::setw(14) << page << std::setw(14) << cur << std::setw(14)
                  << " " + std::to_string(succ[page]) << std::setw(14)
                  << percent(succ[page], total_requests) << "%" << "\n";
    }

    // Dump the number of forced misses to succ_stream
    size_t misses = total_requests - succ[succ.size() - 1];
    succ_stream << std::setw(14) << "Misses" << std::setw(14) << misses << std::setw(14)
                << " " + std::to_string(misses) << std::setw(14) << percent(misses, total_requests)
                << "%" << "\n";

    // dump the stack depth of each page to vec_stream
    std::vector<req_count_t> new_success(succ.size());
    vec_stream << "#" << std::setw(13) << "Req Index" << std::setw(14) << " Stack Depth"
               << "\n";
    for (size_t req = 0; req < distance_vector.size(); req++) {
      if (prev_vec[req] > 0) {
        req_count_t stack_depth = distance_vector[prev_vec[req]];
        if (stack_depth != infinity) {
          vec_stream << std::setw(14) << req << std::setw(14) << " " + std::to_string(stack_depth)
                     << "\n";
          ++new_success[stack_depth];
        }
        else {
          std::cerr << "ERROR: Anything pointed at should not have depth of infinity" << "\n";
        }
      } else {
        vec_stream << std::setw(14) << req << std::setw(14) << " infinity" << "\n";
      }
    }

    // integrate new success function
    size_t sum = 0;
    for (size_t i = 1; i < new_success.size(); i++) {
      sum += new_success[i];
      new_success[i] = sum;
    }

    if (new_success != succ) {
      std::cerr << "ERROR: Success function from stack depths is not equivalent to succ!" << std::endl;
      // for (size_t i = 0; i < succ.size(); i++) {
      //   std::cout << "NEW: " << new_success[i] << " SUCC: " << succ[i] << std::endl;
      // }
    } else {
      std::cerr << "Validated success function built from stack depths is correct!" << std::endl;
    }
  }
#endif  // ALL_METRICS
};

#endif  // ONLINE_CACHE_SIMULATOR_INCLUDE_CACHE_SIM_H_
