
#include <vector>
#include <cmath>

#include "IncrementAndFreeze.h"

class IAKWrapper : public CacheSim {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    // Maximum number of requests in a chunk. Just for accounting purposes
    size_t max_recorded_chunk_size = 0;

    // Vector of stack distances
    std::vector<size_t> distance_histogram; // TODO: Bounded by cM

    // Input to current chunk
    IAKInput chunk_input;

    IncrementAndFreeze iak_alg;

    size_t cur_u = min_u;

    constexpr static size_t max_u_mult = 16; // chunk <= max_u_mult * u
    constexpr static size_t min_u_mult = 8;  // chunk > min_u_mult * u

    constexpr static size_t min_u = 65536; // minimum size of requests array before running IAK

    // Bound on u to limit depth vector to a maximum memory size
    size_t max_living_req = ((size_t)-1) / max_u_mult; // default = unbounded 

    inline size_t get_u() { return cur_u;};

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

    IAKWrapper() = default;
    IAKWrapper(size_t max_memory) : max_living_req(max_memory) {};
    ~IAKWrapper() = default;
};
