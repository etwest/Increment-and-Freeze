#ifndef ONLINE_CACHE_SIMULATOR_PROJECTION_H_
#define ONLINE_CACHE_SIMULATOR_PROJECTION_H_

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>    // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>     // for pair, move, swap
#include <vector>      // for vector, vector<>::iterator
#include <array>       // for array
#include <cmath>        // for ceil

#include "iaf_params.h" // for kIafBranching
#include "op.h"         // for op
#include "partition.h"  // for partitionstate

class PartitionState;

// A sequence of operators defined by a projection
class ProjSequence {
 public:
  std::vector<Op>::iterator op_seq; // iterator to beginning of operations sequence
  req_count_t num_ops;                   // number of operations in this projection

  // Request sequence range
  req_count_t start;
  req_count_t end;
  
  // Initialize an empty projection with bounds (to be filled in by partition)
  ProjSequence(req_count_t start, req_count_t end) : start(start), end(end) {};

  // Init a projection with bounds and iterators
  ProjSequence(req_count_t start, req_count_t end, std::vector<Op>::iterator op_seq, req_count_t num_ops) : 
   op_seq(op_seq), num_ops(num_ops), start(start), end(end) {};

  void partition(ProjSequence& left, ProjSequence& right, req_count_t split_off_idx, PartitionState& state);

  friend std::ostream& operator<<(std::ostream& os, const ProjSequence& seq) {
    os << "start = " << seq.start << " end = " << seq.end << std::endl;
    os << "num_ops = " << seq.num_ops << std::endl;
    os << "Operations: ";
    for (req_count_t i = 0; i < seq.num_ops; i++)
      os << seq.op_seq[i] << " ";
    return os;
  }
};

#endif  // ONLINE_CACHE_SIMULATOR_PROJECTION_H_
