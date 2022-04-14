#ifndef PARAMS_H_GUARD
#define PARAMS_H_GUARD

#define GB (uint64_t)(1 << 30)
#define MB (uint64_t)(1 << 20)
#define KB (uint64_t)(1 << 10)
#define K  (uint64_t)1000
#define M  (uint64_t)1000000

#define SEED        298234433    // seed for the randomness

// params for the representative workload

#define WORKING_SET 10     // number of commonly accessed addresses
#define WORKLOAD    1.5         // size of uncommonly accessed addresses relative to working set
#define LOCALITY    .9          // with what probability to we access a common address
#define ACCESSES    100   // the number of memory accesses

#define ZIPH_FILE   "ziph_data" // where to find the zipfian memory accesses
#define OUT_FILE    "fault_results.txt"

#endif
