#ifndef ONLINE_CACHE_SIMULATOR_PARAMS_H_
#define ONLINE_CACHE_SIMULATOR_PARAMS_H_

constexpr uint64_t kGB = 1 << 30;
constexpr uint64_t kMB = 1 << 20;
constexpr uint64_t kKB = 1 << 10;

constexpr uint64_t kSeed = 298234433;   // seed for the randomness

// general workload parameters
constexpr uint64_t kAccesses       = 500'000'000; // the number of memory accesses
constexpr uint64_t kIdUniverseSize = 20'000'000;  // the total number of unique ids
constexpr uint64_t kMemoryLimit    = 8'000'000;   // memory limit -- depth vector size

// IncrementAndFreeze parameters
constexpr size_t kIafBaseCase      = 256;         // Base case size for IAF algorithm
constexpr size_t kIafBranching     = 16;           // Fanout of each recursive node in 'tree'

// params for the representative workload
constexpr uint64_t kWorkingSet = 50'000; // number of commonly accessed addresses
constexpr double kLocality     = .95;    // with what probability to we access a common address

// error checking
static_assert(kWorkingSet <= kIdUniverseSize);
static_assert(kLocality >= 0 && kLocality <= 1);


#endif  // ONLINE_CACHE_SIMULATOR_PARAMS_H_
