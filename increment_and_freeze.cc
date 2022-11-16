#include "increment_and_freeze.h"

#include <algorithm>
#include <omp.h>
#include <utility>

#include "params.h"

void IncrementAndFreeze::memory_access(uint32_t addr) {
  requests.push_back({addr, (uint32_t) requests.size() + 1});
}

size_t IncrementAndFreeze::populate_operations(
    std::vector<request> &reqs, std::vector<request> *living_req) {

  STARTTIME(sort);
  // sort requests by request id and then by access_number
  std::sort(reqs.begin(), reqs.end());
  STOPTIME(sort);

  // Size of operations array is bounded by 2*reqs
  operations.clear();
  operations.resize(2*reqs.size());
  size_t unique_ids = 0;

  STARTTIME(populate_ops);
#pragma omp parallel reduction(+:unique_ids)
  {
    std::vector<request> living_req_priv;
#pragma omp for nowait // nowait removes the barrier, so the critical copying can happen ASAP
    for (uint64_t i = 0; i < reqs.size(); i++) {
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
  size_t place_idx = 1;
  for (size_t cur_idx = 1; cur_idx < operations.size(); cur_idx++) {
    if (!operations[cur_idx].is_null()) {
      operations[place_idx] = operations[cur_idx];
      place_idx++;
    }
  }
  operations.resize(place_idx); // shrink down to remove nulls at end
  memory_usage = sizeof(Op) * operations.size(); // update memory usage of IncrementAndFreeze
  STOPTIME(populate_ops);

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
  size_t unique_ids = populate_operations(reqs, living_req);

  // Make sure hits_vector has enough space
  if (hits_vector.size() < unique_ids + 1)
    hits_vector.resize(unique_ids + 1);

  // begin the recursive process
  ProjSequence init_seq(1, reqs.size(), operations.begin(), operations.size());

  // We want to spin up a bunch of threads, but only start with 1.
  // More will be added in by do_projections.
  STARTTIME(projections);
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
    // std::cout << "===========================================================" << std::endl;
    // std::cout << "============   Performing New Recursive Step   ============" << std::endl;
    // std::cout << "===========================================================" << std::endl;
    // std::cout << "cur.start = " << cur.start << " cur.end = " << cur.end << std::endl;
    uint64_t dist = cur.end - cur.start + 1;
    double num_partitions = std::min(dist, kIafBranching);

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
  int32_t full_amnt = 0;
  size_t local_distances[kIafBaseCase];
  std::fill(local_distances, local_distances+kIafBaseCase, 0);

  for (uint64_t i = 0; i < cur.num_ops; i++) {
    Op &op = cur.op_seq[i];

    switch(op.get_type()) {
      case Prefix:
        for (uint64_t j = cur.start; j <= op.get_target(); j++)
          local_distances[j - cur.start] += op.get_inc_amnt();
        break;

      case Postfix:
        for (uint64_t j = std::max(op.get_target(), cur.start); j <= cur.end; j++)
          local_distances[j - cur.start] += op.get_inc_amnt();

        // Freeze target by incrementing hits_vector[stack_depth]
        if (op.get_target() != 0) {
          int hit = local_distances[op.get_target() - cur.start] + full_amnt;
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
  uint64_t running_count = 0;
  for (uint64_t i = 1; i < success.size(); i++) {
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
