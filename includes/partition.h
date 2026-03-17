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

#ifndef ONLINE_CACHE_SIMULATOR_PARTITION_H_
#define ONLINE_CACHE_SIMULATOR_PARTITION_H_

#include <cassert>      // for assert
#include <cstddef>      // for size_t
#include <cstdint>      // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>     // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>      // for pair, move, swap
#include <vector>       // for vector, vector<>::iterator
#include <array>        // for array
#include <cmath>        // for ceil

#include "iaf_params.h" // for kIafBranching
#include "op.h"         // for op
#include "projection.h" // for ProjSequence

// on some compilers there is no constexpr log2
// so define it ourselves
constexpr uint64_t ce_log2(uint64_t n) {
  uint64_t ans = 0;
  while (n > 1) {
    ++ans;
    n /= 2;
  }
  return ans;
}

// State that is persisted between calls to partition() at a single node in recursion tree.
class PartitionState {
 private:
  std::array<req_count_t, kIafBranching> incr_array{};
  static constexpr size_t incr_tree_depth = ce_log2(kIafBranching);

 public:
  const double div_factor;
  int all_partitions_full_incr = 0;
  std::array<std::vector<Op>, kIafBranching-1> scratch_spaces;
  int merge_into_idx;
  int cur_idx;
  
  PartitionState(double split, uint64_t num_ops) : 
   div_factor(split), merge_into_idx(num_ops-1), cur_idx(merge_into_idx) {
    for (auto& scratch : scratch_spaces)
      scratch.emplace_back(); // Create an empty null op in each scratch_space
  }

  // Debugging function for printing the incr_array
  void print_incr_array() {
    // print tree
    for (auto elm : incr_array)
      std::cout << elm << " ";
    std::cout << std::endl;
  }

  // Update path to partition_target+1 to represent an increment by 1 in range
  // [partition_target+1, kIafBranching)
  // We represent this tree with the trick that root index = 0
  // left child = cur*2 + 1, right child = cur*2 + 2
  inline req_count_t qry_and_upd_partition_incr(req_count_t partition_target) {
    assert(partition_target < kIafBranching-1);
    size_t depth_shift = incr_tree_depth - 1;
    size_t idx = 0;
    req_count_t sum = 0;

    for (size_t depth = 0; depth < incr_tree_depth; depth++) {
      assert(idx < kIafBranching);
      // if 0 go left, if 1 go right
      size_t leftright = (partition_target & (1 << depth_shift)) >> depth_shift;
      incr_array[idx] += leftright ^ 1;                      // if go left add to tree node value
      sum += incr_array[idx] & ((req_count_t)0 - leftright); // if go right add to sum

      idx = 2*idx + leftright + 1;
      --depth_shift;
    }
    return sum;
  }
};

#endif  // ONLINE_CACHE_SIMULATOR_PARTITION_H_
