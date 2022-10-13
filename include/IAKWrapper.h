
#include <vector>
#include <cmath>

#include "IncrementAndFreeze.h"

class IAKWrapper : public CacheSim {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    // Maximum number of requests in a chunk. Just for accounting purposes
    size_t max_recorded_chunk_size = 0;

    // Vector of stack distances
    std::vector<size_t> distance_histogram;

    // Input to current chunk
    IAKInput chunk_input;

    IncrementAndFreeze iak_alg;

    size_t cur_u;
    size_t max_living_req;

    constexpr static size_t max_u_mult = 16; // chunk <= max_u_mult * u
    constexpr static size_t min_u_mult = 8;  // chunk > min_u_mult * u

    // Function to update value of u given a living requests size
    inline void update_u(size_t num_living) {
      size_t upper_u = max_u_mult * num_living;
      cur_u = num_living * min_u_mult < cur_u ? cur_u : upper_u;
    };

    void process_requests();

  public:
    // Logs a memory access to simulate. The order this function is called in matters.
    void memory_access(uint64_t addr);
    
    /* Returns the success function after processing requests in the current chunk.
     * Does some work, up to u log u depending on the number of unprocessed requests.
     */
    std::vector<size_t> get_success_function();

    inline size_t get_u() { return cur_u; };
    inline size_t get_mem_limit() { return max_living_req; };

    // IAKWrapper Constructor.
    // min_chunk_size: minimum size of requests array before running IAK bigger values of 
    //                 min_chunk_size means more parallelism but more memory consumption.
    // max_cache_size: Limit on the memory sizes for which we report the hit rate. For example a 
    //                 max cache size of 1 GiB means that we report hit rate for all memory sizes
    //                 <= 1 GiB.
    IAKWrapper(size_t min_chunk_size=65536, size_t max_cache_size=((size_t)-1)/max_u_mult) 
      : cur_u(min_chunk_size), max_living_req(max_cache_size) {};
    ~IAKWrapper() = default;
};