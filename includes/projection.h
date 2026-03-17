/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel DeLayo, Bradley Kuszmaul, Evan West
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
