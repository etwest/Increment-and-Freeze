
#include <vector>
#include <cmath>

#include "IncrementAndKillMinInPlace.h"

class IAKWrapper {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    // Vector of stack distances
    std::vector<size_t> distance_histogram;

    // Input to current chunk
    MinInPlace::IAKInput chunk_input;

    MinInPlace::IncrementAndKill iak_alg;

    constexpr static size_t u_mult = 8;  // how much bigger than living requests the requests array is
    constexpr static size_t u_mult_mult = 2;  // How much extra space we take to avoid realloc in vector

    constexpr static size_t min_u = 2048; // minimum size of requests array before running IAK

    size_t cur_u = min_u;

    inline size_t get_u() { return cur_u;};
    inline void update_u() { 
      cur_u = std::max(
         (size_t) ceil((double)(chunk_input.output.living_requests.size() * u_mult) / u_mult_mult) * u_mult_mult,
         min_u); 
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
