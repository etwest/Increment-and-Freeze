
#include <vector>

#include "IncrementAndKillMinInPlace.h"

class IAKWrapper {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    // A vector of currents requests
    std::vector<tuple> requests;

    // A vector of living requests
    std::vector<tuple> living;

    // Vector of stack distances
    std::vector<size_t> distance_histogram;

    MinInPlace::IncrementAndKill iak_alg;

    constexpr static size_t u_mult = 4;  // how much bigger than living requests the requests array is
    constexpr static size_t min_u = 128; // minimum size of requests array before running IAK

    inline size_t get_u() { return std::max(min_u, living.size() * u_mult); };

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
