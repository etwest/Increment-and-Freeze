#ifndef ONLINE_CACHE_SIMULATOR_PARAMS_H_
#define ONLINE_CACHE_SIMULATOR_PARAMS_H_

constexpr uint64_t kSeed = 298234433;   // seed for the randomness

// general workload parameters
// constexpr uint64_t kAccesses       = 10'000'000'000; // the number of memory accesses
// constexpr uint64_t kIdUniverseSize = 268'435'456;   // the total number of unique ids
// constexpr uint64_t kMemoryLimit    = 67'108'864;    // memory limit -- depth vector size

constexpr uint64_t kAccesses       = 10'000'000; // the number of memory accesses
constexpr uint64_t kIdUniverseSize = 200'000;   // the total number of unique ids
constexpr uint64_t kMemoryLimit    = 75'000;   // memory limit -- depth vector size

// params for the representative workload
constexpr uint64_t kWorkingSet = 50'000; // number of commonly accessed addresses
constexpr double kLocality     = .95;    // with what probability to we access a common address

// error checking
static_assert(kWorkingSet <= kIdUniverseSize);
static_assert(kLocality >= 0 && kLocality <= 1);


#endif  // ONLINE_CACHE_SIMULATOR_PARAMS_H_
