#ifndef ONLINE_CACHE_SIMULATOR_PARTITION_H_
#define ONLINE_CACHE_SIMULATOR_PARTITION_H_

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>    // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>     // for pair, move, swap
#include <vector>      // for vector, vector<>::iterator
#include <array>       // for array
#include <cmath>        // for ceil

#include "params.h"     // for kIafBranching
#include "op.h"         // for op
#include "projection.h" // for ProjSequence

// State that is persisted between calls to partition() at a single node in recursion tree.
class PartitionState {
 private:
  struct incr_array_node {
    uint32_t value = 0;
    uint32_t key = 0;
  };
  std::array<incr_array_node, kIafBranching-1> incr_array = std::move(construct_tree(kIafBranching-1));

  constexpr std::array<incr_array_node, kIafBranching-1> construct_tree(size_t num_items) {
    std::array<incr_array_node, kIafBranching-1> local_incr_array;
    helper_construct_tree(local_incr_array, num_items, 0, 0);
    return local_incr_array;
  }

  constexpr void helper_construct_tree(std::array<incr_array_node, kIafBranching-1>& local_incr_array, size_t num_items, size_t index, size_t key_offset) {
    if (num_items == 0) return;

    size_t node_key = num_items / 2 + key_offset;
    local_incr_array[index].key = node_key;
    local_incr_array[index].value = 0;

    helper_construct_tree(local_incr_array, num_items / 2, 2*index + 1, key_offset);
    helper_construct_tree(local_incr_array, num_items - num_items / 2 - 1, 2*index + 2, node_key + 1);
  }

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
      std::cout << elm.key << "," << elm.value << " ";
    std::cout << std::endl;
  }

  // Update path to partition_target+1 to represent an increment by 1 in range
  // [partition_target+1, kIafBranching)
  // We represent this tree with the trick that root index = 0
  // left child = cur*2 + 1, right child = cur*2 + 2
  inline void upd_partition_incr(size_t partition_target) {
    size_t incr_target = partition_target + 1;
    if (incr_target >= kIafBranching-1) return;

    size_t idx = 0;


    while (incr_array[idx].key != incr_target) {
      assert(idx < kIafBranching);
      if (incr_array[idx].key < incr_target)
        idx = 2*idx + 2; // go right
      else {
        // update node value and go left
        ++incr_array[idx].value;
        idx = 2*idx + 1;
      }
    }

    // finally, update the value of the incr_target node
    ++incr_array[idx].value;
  }

  // Return the sum of the increments on path to target to get their affect
  inline size_t qry_partition_incr(size_t partition_target) {
    if (partition_target == 0) return 0;
    assert(partition_target < kIafBranching-1);
    size_t sum = 0;
    size_t idx = 0;
    while (incr_array[idx].key != partition_target) {
      assert(idx < kIafBranching);
      if (incr_array[idx].key < partition_target) {
        // add to sum and go right.
        sum += incr_array[idx].value;
        idx = 2*idx + 2;
      }
      else
        idx = 2*idx + 1; // go left
    }

    // finally, add value of partition_target node
    return sum + incr_array[idx].value;
  }
};

#endif  // ONLINE_CACHE_SIMULATOR_PARTITION_H_
