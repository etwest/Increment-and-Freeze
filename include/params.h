#ifndef PARAMS_H_GUARD
#define PARAMS_H_GUARD

#define GB (uint64_t)(1 << 30)
#define MB (uint64_t)(1 << 20)
#define KB (uint64_t)(1 << 10)
#define K  (uint64_t)1000
#define M  (uint64_t)1000000

#define SEED        29873433    // seed for the randomness

#define MEM_SIZE    (GB)        // size of main memory
#define PAGE_SIZE   (KB << 2)   // size of a page

// params for the representative workload
#define WORKING_SET (MB << 9)   // size of the commonly accessed pages
#define WORKLOAD    1.5         // size of uncommonly accessed pages relative to working set
#define LOCALITY    .9          // with what probability to we access a common page
#define ACCESSES    (1 * M)     // the number of memory accesses

#define ZIPH_FILE   "ziph_data" // where to find the zipfian memory accesses
#define OUT_FILE    "fault_results.txt"

#endif
