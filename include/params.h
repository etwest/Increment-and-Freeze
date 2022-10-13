#ifndef PARAMS_H_GUARD
#define PARAMS_H_GUARD

constexpr uint64_t GB = 1 << 30;
constexpr uint64_t MB = 1 << 20;
constexpr uint64_t KB = 1 << 10;

constexpr uint64_t SEED = 298234433;   // seed for the randomness

// general workload parameters
constexpr uint64_t ACCESSES    = 1e7;  // the number of memory accesses
constexpr uint64_t UNIVERSE_SIZE  = 1e7;  // number of unique ids (the size of the universe)

// params for the representative workload
constexpr uint64_t WORKING_SET = 5e4;  // number of commonly accessed addresses
constexpr double LOCALITY      = .95;  // with what probability to we access a common address

// error checking
static_assert(WORKING_SET <= UNIVERSE_SIZE);
static_assert(LOCALITY >= 0 && LOCALITY <= 1);

#endif
