#ifndef PARAMS_H_GUARD
#define PARAMS_H_GUARD

constexpr uint64_t GB = 1 << 30;
constexpr uint64_t MB = 1 << 20;
constexpr uint64_t KB = 1 << 10;

constexpr uint64_t SEED = 298234433;   // seed for the randomness

// general workload parameters
constexpr uint64_t ACCESSES    = 1e7;  // the number of memory accesses
constexpr uint64_t UNIQUE_IDS  = 1e7;  // the total number of unique ids

// params for the representative workload
constexpr uint64_t WORKING_SET = 5e4;  // number of commonly accessed addresses
constexpr double LOCALITY      = .95;  // with what probability to we access a common address
static_assert(WORKING_SET <= UNIQUE_IDS);
static_assert(LOCALITY >= 0 && LOCALITY <= 1);

const std::string OUT_FILE = "fault_results.txt";

#endif
