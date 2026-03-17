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

#include "projection.h"

void ProjSequence::partition(ProjSequence& left, ProjSequence& right, req_count_t split_off_idx, PartitionState& state) {
  // pull relevant stuff out of PartitionState
  const double div_factor        = state.div_factor;
  int& all_partitions_full_incr  = state.all_partitions_full_incr;
  auto& partition_scratch_spaces = state.scratch_spaces;
  int& merge_into_idx            = state.merge_into_idx;
  int& cur_idx                   = state.cur_idx;


  // std::cout << "Performing partition upon projected sequence" << std::endl;
  // std::cout << *this << std::endl;
  // std::cout << "cur_idx = " << cur_idx << " merge_into_idx = " << merge_into_idx << std::endl;
  // std::cout << "Partitioning into: " << left.start << "-" << left.end << ", ";
  // std::cout << right.start << "-" << right.end << std::endl;
  // std::cout << std::endl;

  assert(left.start <= left.end);
  assert(left.end+1 == right.start);
  assert(right.start <= right.end);
  assert(start == left.start);
  assert(end == right.end);
  assert(op_seq[0].is_null());

  // Where we merge operations that remain on the right side
  // use ints for this and cur_idx because underflow is good and tells us things

  // loop through all the operations on the right side
  for (; cur_idx >= 0; cur_idx--) {
    Op& op = op_seq[cur_idx];

    assert(op.get_type() != Prefix || op.get_target() >= left.end);

    if (op.is_boundary_op(left.end)) {
      // std::cout << op << " is a BOUNDARY OP" << std::endl;
      // we merge this op with the next left op (also need to add inc amount to full)
      // The previous OP is the end of the scratch space
      Op& prev_op = op_seq[cur_idx-1];
      prev_op.add_full(op.get_full_amnt() + op.get_inc_amnt());

      // AND merge this op with merge_into_idx
      // if merge_into_idx == cur_idx then
      //   then leave a null op here with our full inc amount
      if (merge_into_idx == cur_idx)
        op.make_null();
      else {
        assert(op_seq[merge_into_idx].is_null());
        op_seq[merge_into_idx].add_full(op.get_full_amnt());

        // make this boundary_op have no_impact
        op = Op();
      }

      // done processing
      --cur_idx;
      break;
    }

    if (op.move_to_scratch(right.start)) {
      // std::cout << "MOVING " << op << " left!" << std::endl;

      // 1. Identify the partition this Postfix is targeting (inverting the parition map)
      req_count_t partition_target = ceil((op.get_target() - (start-1)) / div_factor) - 1;
      assert(partition_target < split_off_idx);

      // 2. Place this Postfix in the appropriate scratch space.
      //    the null at the end of the scratch_stack gives a sum of the full increments
      //    already applied to this partition
      std::vector<Op>& scratch_stack = partition_scratch_spaces[partition_target];
      assert(scratch_stack.back().is_null());
      
      // query for Postfix increments that are full increments in this partition and incr them
      req_count_t incrs = state.qry_and_upd_partition_incr(partition_target);
      req_count_t stack_full_incr_sum = scratch_stack.back().get_full_amnt();
      scratch_stack.back() = op;
      scratch_stack.back().add_full(incrs + all_partitions_full_incr - stack_full_incr_sum);

      // 3. Add to all_partitions_full_incr
      all_partitions_full_incr += op.get_full_amnt();

      // 4. save the amount of full we've already added to target_partition in a new Null op
      scratch_stack.emplace_back(); // add a new empty Null to end of scratch stack
      scratch_stack.back().add_full(incrs + all_partitions_full_incr); // set the full

      // 5. At this point, we've projected op into [placement, last). 
      //    Now let's fix the value in last (split_off_idx).
      //    Op should be unmodified at this point.
      if (cur_idx != merge_into_idx) {
        // merge this operation with merge_into_idx and make this op no impact
        op_seq[merge_into_idx].add_full(op.get_full_amnt() + op.get_inc_amnt());
        op = Op(); // make empty
      } else {
        // make this operation null and add incr amount to full
        op.add_full(op.get_inc_amnt());
        op.make_null();
      }
    }
    else {
      // std::cout << op << " stays on right side." << std::endl;
      // we don't move this op to left but does it affect the full incr of other partitions
      all_partitions_full_incr += op.get_full_incr_to_left(right.start);

      if (merge_into_idx != cur_idx) {
        // merge current op into merge idx op
        req_count_t full = op_seq[merge_into_idx].get_full_amnt();
        op.add_full(full);
        op_seq[merge_into_idx] = op;
        op = Op(); // set where op used to be to a no_impact operation
      }
      // if moved operation is not null then we need to decr merge_into_idx
      if (!op_seq[merge_into_idx].is_null()) merge_into_idx--;
    }

    // // Print out operations
    // std::cout << "all_partitions_full_incr: " << all_partitions_full_incr << std::endl;
    // std::cout << "cur_idx = " << cur_idx - 1 << " merge_into_idx = " << merge_into_idx << std::endl;
    // std::cout << *this << std::endl;

    // // print out scratch stack
    // std::cout << "Scratch_stack: " << std::endl;
    // for (auto &scratch_stack : partition_scratch_spaces) {
    //   for (auto op : scratch_stack)
    //     std::cout << op << " ";
    //   std::cout << std::endl;
    // }
    // std::cout << std::endl;
  }
  assert(cur_idx >= 0);

  // // Print out operations
  // std::cout << "all_partitions_full_incr: " << all_partitions_full_incr << std::endl;
  // std::cout << "Done processing projection" << std::endl;
  // std::cout << "cur_idx = " << cur_idx << " merge_into_idx = " << merge_into_idx << std::endl;
  // std::cout << *this << std::endl;

  // // print out scratch stack
  // std::cout << "Scratch_stack: " << std::endl;
  // for (auto &scratch_stack : partition_scratch_spaces) {
  //   for (auto op : scratch_stack)
  //     std::cout << op << " ";
  //   std::cout << std::endl;
  // }
  // std::cout << std::endl;

  // Partition the operations between the left and the right
  // left should include all operations up to merge_into_idx
  // right is all operations after that
  left.op_seq  = op_seq;
  left.num_ops = merge_into_idx;
  assert(left.op_seq[0].is_null());

  right.op_seq = left.op_seq + left.num_ops;
  right.num_ops = num_ops - left.num_ops;
  assert(right.op_seq[0].is_null());

  // Merge in scratch_stack for leftmost partition scratch spaces
  // Iterate through these from front to back while walking the merge_into_idx
  // to the left.
  std::vector<Op>& scratch_stack = partition_scratch_spaces[split_off_idx - 1];
  assert(scratch_stack.size() > 0 && scratch_stack.back().is_null());
  assert(merge_into_idx - cur_idx >= (int) scratch_stack.size());
  for (req_count_t i = 0; i < scratch_stack.size() - 1; i++) {
    --merge_into_idx;
    // std::cout << op_seq[merge_into_idx] << " <- " << scratch_stack[i] << std::endl;
    op_seq[merge_into_idx] = scratch_stack[i];
  }
  // The last op in the scratch_stack is a Null that defines the amount we should
  // add to all_partitions_full_incr to define an additional full increment
  Op& back = scratch_stack.back();
  req_count_t incrs_to_end = state.qry_and_upd_partition_incr(split_off_idx - 1);
  merge_into_idx--;
  op_seq[merge_into_idx].add_full(all_partitions_full_incr + incrs_to_end - back.get_full_amnt());
  scratch_stack.clear();

#ifndef NDEBUG
  // Calculate the number of Postfixes that are unresolved
  // Ensure there is enough space for them in future partitions
  int unresolved_postfixes = 0;
  // std::cout << "split_off_idx = " << split_off_idx << std::endl;
  for (req_count_t i = 0; i < split_off_idx - 1; i++) {
    // std::cout << "Size of " << i << " = " << partition_scratch_spaces[i].size() << std::endl;
    unresolved_postfixes += partition_scratch_spaces[i].size() - 1;
  }
  // assert there will be enough space for these unresolved postfixes
  // in the future
  // std::cout << "merge_into_idx = " << merge_into_idx << " cur_idx = " << cur_idx << std::endl;
  // std::cout << "unresolved_postfixes: " << unresolved_postfixes << std::endl;
  assert(merge_into_idx - unresolved_postfixes == cur_idx);
#endif

  // Print out final projection
  // std::cout << "Final Result: " << std::endl;
  // std::cout << "LEFT:  " << left << std::endl;
  // std::cout << "RIGHT: " << right << std::endl << std::endl;
}
