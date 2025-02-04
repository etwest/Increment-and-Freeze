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

#include "increment_and_freeze.h"

#include <algorithm>
//#include <omp.h>
#include <utility>

void IncrementAndFreeze::memory_access(req_count_t addr) {
  ++access_number;
  requests.push_back({addr, (req_count_t) requests.size() + 1});
}

req_count_t IncrementAndFreeze::populate_operations(
    std::vector<request> &reqs, std::vector<request> *living_req) {

  reqs.resize(reqs.size()); // get rid of empty requests to save memory

  STARTTIME(sort_requests);
  // sort requests by request id and then by access_number
  std::sort(reqs.begin(), reqs.end());
  STOPTIME(sort_requests);

  // Size of operations array is bounded by 2*reqs
  operations.clear();
  STARTTIME(allocate_ops)
  operations.resize(2*reqs.size());
  STOPTIME(allocate_ops);

  STARTTIME(build_op_array);
  req_count_t unique_ids = 0;
#pragma omp parallel reduction(+:unique_ids)
  {
    std::vector<request> living_req_priv;
#pragma omp for nowait // nowait removes the barrier, so the critical copying can happen ASAP
    for (req_count_t i = 0; i < reqs.size(); i++) {
      auto [addr, access_num] = reqs[i];
      auto [last_addr, last_access_num] = i == 0 ? request(0, 0): reqs[i-1];

      // Using last, check if previous sorted access is the same
      if (last_access_num > 0 && addr == last_addr) {
        // prev is same id as us so create Prefix and Postfix
        operations[2*access_num-2] = Op(access_num-1, -1); // Prefix  i-1, +1, Full -1
        operations[2*access_num-1] = Op(last_access_num);  // Postfix prev(i), +1, Full 0
      }
      else {
        // previous access is different. This is therefore first access to this id
        // so only create Prefix.
        operations[2*access_num-2] = Op(access_num-1, 0); // Prefix  i-1, +1, Full 0
        ++unique_ids;

        // The previous request survives this chunk so add to living
        if (living_req != nullptr && i > 0) {
          living_req_priv.push_back(reqs[i-1]);
        }
      }
    }
    if (living_req != nullptr)
    {
#pragma omp critical
      living_req->insert(living_req->end(), 
          std::make_move_iterator(living_req_priv.begin()), 
          std::make_move_iterator(living_req_priv.end()));
    }
  }
  // Compact operations vector
  req_count_t place_idx = 1;
  for (req_count_t cur_idx = 1; cur_idx < operations.size(); cur_idx++) {
    if (!operations[cur_idx].is_null()) {
      operations[place_idx] = operations[cur_idx];
      place_idx++;
    }
  }
  operations.resize(place_idx); // shrink down to remove nulls at end
  memory_usage = sizeof(Op) * operations.size(); // update memory usage of IncrementAndFreeze
  STOPTIME(build_op_array);

  if (living_req == nullptr)
    return unique_ids;

  // very last item in reqs is an edge case. Manually add here
  living_req->push_back(reqs[reqs.size()-1]);

  // sort by access number
  STARTTIME(sort_new_living);
  std::sort(living_req->begin(), living_req->end(), [](auto &left, auto &right) {
    return left.access_number < right.access_number;
  });
  STOPTIME(sort_new_living);
  return unique_ids;
}

// 'Main' function of IAF. Used to update a hits vector given a vector of requests
void IncrementAndFreeze::update_hits_vector(std::vector<request>& reqs,
  SuccessVector& hits_vector, std::vector<request> *living_req) {
  STARTTIME(update_hits_vector);
  STARTTIME(create_operations)
  req_count_t unique_ids = populate_operations(reqs, living_req);
  STOPTIME(create_operations);

  STARTTIME(resize_hits_vector);
  // Make sure hits_vector has enough space
  if (hits_vector.size() < unique_ids + 1)
    hits_vector.resize(unique_ids + 1);
  STOPTIME(resize_hits_vector);

  // begin the recursive process
  STARTTIME(projections);
  ProjSequence init_seq(1, reqs.size(), operations.begin(), operations.size());

  // We want to spin up a bunch of threads, but only start with 1.
  // More will be added in by do_projections.
#pragma omp parallel
#pragma omp single
  do_projections(hits_vector, std::move(init_seq));

  STOPTIME(projections);
  STOPTIME(update_hits_vector);

  // Print out hits vector for debugging
  // std::cout << "Hits Vector: ";
  // for (auto hit : hits_vector)
  //   std::cout << hit << " ";
  // std::cout << std::endl;
}

//recursively (and in parallel) perform all the projections
void IncrementAndFreeze::do_projections(SuccessVector& hits_vector, ProjSequence cur) {
  // base case
  // brute force algorithm to solve problems of size <= kIafBaseCase
  if (cur.end - cur.start < kIafBaseCase) {
    do_base_case(hits_vector, cur);
    return;
  }
  else {
    req_count_t dist = cur.end - cur.start + 1;
    double num_partitions = std::min(dist, (req_count_t) kIafBranching);

    // This biased toward making right side projects larger which is good
    // because they shrink while left gets bigger
    double split_amount = dist / num_partitions;
    double fractional_end = cur.end;
    // std::cout << "distance = " << dist << " partitions = " << num_partitions << " split_amnt = " << split_amount << std::endl;

    PartitionState state(split_amount, cur.num_ops);

    // split off a portion of the projected sequence
    ProjSequence remaining_sequence(0,0);
    for (size_t i = num_partitions - 1; i > 0; i--) {
      fractional_end -= split_amount;
      // std::cout << "fractional_end = " << fractional_end << std::endl;
      assert(fractional_end >= cur.start);

      // split off rightmost portion of current sequence
      ProjSequence split_sequence(fractional_end + 1, cur.end);
      remaining_sequence = std::move(ProjSequence(cur.start, fractional_end));
      cur.partition(remaining_sequence, split_sequence, i, state);
      cur = std::move(remaining_sequence);

      // create a task to process split off sequence
#pragma omp task shared(hits_vector) mergeable final(dist <= 8192)
      do_projections(hits_vector, std::move(split_sequence));
    }

    // process remaining projected sequence
    do_projections(hits_vector, std::move(cur));
  }
}

void IncrementAndFreeze::do_base_case(SuccessVector& hits_vector, ProjSequence cur) {
  int64_t full_amnt = 0;
  size_t local_distances[kIafBaseCase];
  std::fill(local_distances, local_distances+kIafBaseCase, 0);

  for (req_count_t i = 0; i < cur.num_ops; i++) {
    Op &op = cur.op_seq[i];

    switch(op.get_type()) {
      case Prefix:
        for (req_count_t j = cur.start; j <= op.get_target(); j++)
          local_distances[j - cur.start] += op.get_inc_amnt();
        break;

      case Postfix:
        for (req_count_t j = std::max(op.get_target(), cur.start); j <= cur.end; j++)
          local_distances[j - cur.start] += op.get_inc_amnt();

        // Freeze target by incrementing hits_vector[stack_depth]
        if (op.get_target() != 0) {
          int64_t hit = local_distances[op.get_target() - cur.start] + full_amnt;
          // std::cout << "Freezing " << op << " = " << hit << std::endl;
          assert(hit > 0);
          assert((size_t)hit < hits_vector.size());
#pragma omp atomic update
          hits_vector[hit]++;
        }
        break;

      default: // Null
        break;
    }

    // Add full amount
    full_amnt += op.get_full_amnt();
  }
}

CacheSim::SuccessVector IncrementAndFreeze::get_success_function() {
  STARTTIME(get_success_fnc);

  // hits[x] tells us the number of requests that are hits for all memory sizes >= x
  SuccessVector success;
  update_hits_vector(requests, success);

  STARTTIME(sequential_prefix_sum);
  // integrate to convert to success function
  req_count_t running_count = 0;
  for (req_count_t i = 1; i < success.size(); i++) {
    running_count += success[i];
    success[i] = running_count;
  }
  STOPTIME(sequential_prefix_sum);
  STOPTIME(get_success_fnc);
  return success;
}

void IncrementAndFreeze::process_chunk(ChunkInput &input) {
  input.output.living_requests.clear();
  update_hits_vector(input.requests, input.output.hits_vector, &input.output.living_requests);
}
