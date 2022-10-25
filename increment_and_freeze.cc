#include "increment_and_freeze.h"

#include <algorithm>
#include <omp.h>
#include <utility>

#include "params.h"

void IncrementAndFreeze::memory_access(uint64_t addr) {
  requests.push_back({addr, access_number++});
}

void IncrementAndFreeze::calculate_prevnext(
    std::vector<req_index_pair> &req, std::vector<req_index_pair> *living_req) {

  // put all requests of the same addr next to each other
  // then order those by access_number
  std::vector<req_index_pair> requestcopy = req; // MEMORY_ALLOC (operator= for vector)

  STARTTIME(sort);
  std::sort(requestcopy.begin(), requestcopy.end());
  STOPTIME(sort);

  prev_arr.resize(requestcopy.size() + 1); // good memory allocation, persists between calls and based on size of requests arr

  STARTTIME(prevnext);
#pragma omp parallel
  {
    std::vector<req_index_pair> living_req_priv;
#pragma omp for nowait // nowait removes the barrier, so the critical copying can happen ASAP
    for (uint64_t i = 0; i < requestcopy.size(); i++) {
      auto [addr, access_num] = requestcopy[i];
      auto [last_addr, last_access_num] = i == 0 ? req_index_pair(0, 0): requestcopy[i-1];

      // Using last, check if previous sorted access is the same
      if (last_access_num > 0 && addr == last_addr) {
        prev(access_num) = last_access_num;  // Point access to previous
      } else {
        prev(access_num) = 0;  // last is different so prev = 0
        if (living_req != nullptr && i > 0) { // add last to living requests
          living_req_priv.push_back(requestcopy[i-1]);
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
  STOPTIME(prevnext);

  if (living_req == nullptr)
    return;

  // very last item in requestcopy is an edge case
  // manually add here
  living_req->push_back(requestcopy[requestcopy.size()-1]);

  // sort by access number
  STARTTIME(sort_living);
  std::sort(living_req->begin(), living_req->end(), [](auto &left, auto &right){return left.second < right.second; });
  STOPTIME(sort_living);
}

std::vector<uint64_t> IncrementAndFreeze::get_distance_vector() {
  STARTTIME(getdistancevector);
  std::vector<uint64_t> distance_vector(requests.size()+1);

  // update memory usage of IncrementAndFreeze
  memory_usage = sizeof(Op) * 4 * requests.size();

  // Generate the list of operations
  // Here, we init enough space for all operations.
  // Every kill is either a kill or not
  // Every subrange increment can expand into at most 2 non-passive ops
  // std::cout << "MIP Requesting memory: " 
  //           << sizeof(Op) * 2 * 2 * requests.size() * 1.0 / kGB 
  //           << " GB" << std::endl;
  operations.clear();
  operations.resize(2*requests.size());
  scratch.clear();
  scratch.resize(2*requests.size());

  // Increment(prev(i)+1, i-1, 1)
  // Freeze(prev(i))

  // We encode the above with:

  // Prefix Inc i-1, 1
  // Full increment -1
  // Freeze prev(i)
  // Suffix Increment prev(i)

  // Note: 0 index vs 1 index
  for (uint64_t i = 0; i < requests.size(); i++) {
    operations[2*i] = Op(i, -1); // Prefix i, +1, Full -1        
    operations[2*i+1] = Op(prev(i+1));
  }

  // begin the recursive process
  ProjSequence init_seq(1, requests.size(), operations.begin(), scratch.begin(), operations.size(), operations.size());

  // We want to spin up a bunch of threads, but only start with 1.
  // More will be added in by do_projections.
#pragma omp parallel
#pragma omp single
  do_projections(distance_vector, std::move(init_seq));

  STOPTIME(getdistancevector);
  return distance_vector;
}

void IncrementAndFreeze::get_depth_vector(IAKInput &chunk_input) {
  STARTTIME(getdepthvector);

  size_t living_size = chunk_input.output.living_requests.size();

  IAKOutput &ret = chunk_input.output;
  ret.living_requests.clear();
  calculate_prevnext(chunk_input.chunk_requests, &(ret.living_requests));

  STARTTIME(memory_allocs);
  ret.depth_vector.clear();
  ret.depth_vector.resize(chunk_input.chunk_requests.size() + 1); // MEMORY_ALLOC (resize)

  // Generate the list of operations
  // Here, we init enough space for all operations.
  // Every kill is either a kill or not
  // Every subrange increment can expand into at most 2 non-passive ops
  size_t arr_size = 2 * (chunk_input.chunk_requests.size());

  // update memory usage of IncrementAndFreeze
  memory_usage = sizeof(Op) * 2 * arr_size;

  operations.clear();
  operations.resize(arr_size); // MEMORY_ALLOC
  scratch.clear();
  scratch.resize(arr_size); // MEMORY_ALLOC

  // Increment(prev(i)+1, i-1, 1)
  // Freeze(prev(i))

  // We encode the above with:

  // Prefix Inc i-1, 1
  // Full increment -1
  // Freeze prev(i)
  // Suffix Increment prev(i)

  // Null requests to give space for indices where living requests reside
  STOPTIME(memory_allocs);

  STARTTIME(living_populate);
  // Note: 0 index vs 1 index
  for (uint64_t i = living_size; i < chunk_input.chunk_requests.size(); i++) {
    auto[request_id, request_index] = chunk_input.chunk_requests[i];
    operations[2*i] = Op(request_index - 1, -1); // Prefix i-1, Full -1
    operations[2*i+1] = Op(prev(request_index)); // Postfix prev(i), Full +0 (freeze before +full)
  }
  STOPTIME(living_populate);

  size_t max_index = chunk_input.chunk_requests[chunk_input.chunk_requests.size() - 1].second;

  // begin the recursive process
  ProjSequence init_seq(1, max_index, operations.begin(), scratch.begin(), operations.size(), operations.size());

  // We want to spin up a bunch of threads, but only start with 1.
  // More will be added in by do_projections.
  STARTTIME(projections);
#pragma omp parallel
#pragma omp single
  do_projections(ret.depth_vector, std::move(init_seq));

  STOPTIME(projections);
  STOPTIME(getdepthvector);
}

//recursively (and in parallel) perform all the projections
void IncrementAndFreeze::do_projections(std::vector<uint64_t>& distance_vector, ProjSequence cur) {
  // base case
  // brute force algorithm to solve problems of size <= kIafBaseCase
  if (cur.end - cur.start < kIafBaseCase) {
    do_base_case(distance_vector, cur);
    return;
  }
  else {
    uint64_t dist = cur.end - cur.start;
    uint64_t mid = (dist) / 2 + cur.start;

    // generate projected sequence for first half
    ProjSequence fst_half(cur.start, mid);
    ProjSequence snd_half(mid + 1, cur.end);

    cur.partition(fst_half, snd_half);

#pragma omp task shared(distance_vector) mergeable final(dist <= 1024) 
    do_projections(distance_vector, std::move(fst_half));

    do_projections(distance_vector, std::move(snd_half));
  }
}

void IncrementAndFreeze::do_base_case(std::vector<uint64_t>& distance_vector, ProjSequence cur) {
  bool frozen[kIafBaseCase];
  std::fill(frozen, frozen+kIafBaseCase, false);

  for (uint64_t i = 0; i < cur.len; i++) {
    Op &op = cur.op_seq[i];

    if (op.get_type() == Prefix)
      for (uint64_t j = cur.start; j < op.get_target() + 1; j++)
        if (!frozen[j - cur.start]) distance_vector[j] += op.get_inc_amnt();

    if (op.get_type() == Postfix)
      for (uint64_t j = op.get_target(); j < cur.end + 1; j++) {
        if (j == 0) continue;
        if (!frozen[j - cur.start]) distance_vector[j] += op.get_inc_amnt();
      }

    if (op.get_type() == Postfix && op.get_target() != 0)
      frozen[op.get_target() - cur.start] = true;

    if (op.get_full_amnt() != 0) {
      for (uint64_t j = cur.start; j < cur.end + 1; j++)
        if (!frozen[j - cur.start]) distance_vector[j] += op.get_full_amnt();
    }
  }
}

std::vector<uint64_t> IncrementAndFreeze::get_success_function() {
  STARTTIME(get_success_fnc);
  calculate_prevnext(requests);
  auto distances = get_distance_vector();

  STARTTIME(converting_distances_to_succ);
  // a point representation of successes
  std::vector<uint64_t> success(distances.size());
#pragma omp parallel for //reduction(vec_uint64_t_plus : success)
  for (uint64_t i = 0; i < distances.size() - 1; i++) {
    // std::cout << distances[i + 1] << " ";
    if (prev(i + 1) == 0) continue;
    auto index = distances[prev(i+1)];
#pragma omp atomic update
    success[index]++;
  }
  // std::cout << std::endl;
  STOPTIME(converting_distances_to_succ);

  STARTTIME(sequential_prefix_sum);
  // integrate
  uint64_t running_count = 0;
  for (uint64_t i = 1; i < success.size(); i++) {
    running_count += success[i];
    success[i] = running_count;
  }
  STOPTIME(sequential_prefix_sum);
  STOPTIME(get_success_fnc);
  return success;
}

// Create a new Operation by projecting another
Op::Op(const Op& oth_op, uint64_t proj_start, uint64_t proj_end) {
  _target = oth_op._target;
  full_amnt = oth_op.full_amnt;
  switch(oth_op.type()) {
    case Prefix: //we affect before target
      if (proj_start > target())
      {
        set_type(Null);
      }
      else if (proj_end <= target()) //full inc case
      {
        set_type(Null);
        full_amnt += oth_op.inc_amnt;
      }
      else // prefix case
      {
        set_type(Prefix);
      }
      return;
    case Postfix: //we affect after target
      if (proj_end < target())
      {
        set_type(Null);
      }
      else if (proj_start > target()) //full inc case
      { // We can't collapse if the target is in the range-- we still need the kill information
        set_type(Null);
        full_amnt += oth_op.inc_amnt;
      }
      else // Postfix case
      {
        set_type(Postfix);
      }
      return;
    case Null:
      set_type(Null);
      return;
    default: assert(false); return;
  }
}
