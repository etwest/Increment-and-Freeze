
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

    size_t cur_u = min_u;

    constexpr static size_t max_u_mult = 16; // chunk <= max_u_mult * u
    constexpr static size_t min_u_mult = 8;  // chunk > min_u_mult * u

    constexpr static size_t min_u = 65536; // minimum size of requests array before running IAK

    inline size_t get_u() { return cur_u;};
    inline void update_u() {
      size_t upper_u = max_u_mult * chunk_input.output.living_requests.size();
      cur_u = chunk_input.output.living_requests.size() * min_u_mult < cur_u ? cur_u : upper_u;
    };

    void process_requests();

  public:
    // Logs a memory access to simulate. The order this function is called in matters.
    void memory_access(uint64_t addr);
    
    /* Returns the success function.
     * Does *a lot* of work.
     * When calling print_success_function, the answer is re-computed.
     */
    std::vector<size_t> get_success_function();
    IAKWrapper() = default;
    ~IAKWrapper() = default;
};
