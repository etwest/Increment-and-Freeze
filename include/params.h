#ifndef PARAMS_H_GUARD
#define PARAMS_H_GUARD

#define GB (uint64_t)(1 << 30)
#define MB (uint64_t)(1 << 20)
#define KB (uint64_t)(1 << 10)
#define K  (uint64_t)1000
#define M  (uint64_t)1000000

#define SEED        29873433    // seed for the randomness

#define MEM_SIZE    (MB << 4)   // size of main memory
#define PAGE_SIZE   (KB << 2)   // size of a page

// params for the representative workload
#define WORKING_SET .95         // the size of the commonly accessed pages relative to main memory
#define WORKLOAD    1.5         // the total size of all accessed pages relative to main memory
#define LOCALITY    .9          // with what probability to we access a common page
#define ACCESSES    (100 * K)   // the number of memory accesses

#define ZIPH_FILE   "ziph_data" // where to find the zipfian memory accesses

#endif
