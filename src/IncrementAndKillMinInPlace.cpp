#include "IncrementAndKillMinInPlace.h"

#include <algorithm>
#include <queue>
#include <omp.h>
#include "params.h"
#include <utility>
#include <chrono>
class IncrementAndKill;

namespace MinInPlace {

  void IncrementAndKill::memory_access(uint64_t addr) {
    requests.push_back({addr, access_number++});
  }

  std::vector<IncrementAndKill::req_index_pair> IncrementAndKill::calculate_prevnext(bool calc_living) {
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    // put all requests of the same addr next to each other
    // then order those by access_number
    auto requestcopy = requests;

    auto start = high_resolution_clock::now();
    std::sort(requestcopy.begin(), requestcopy.end());
    auto sort_time =  duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
    
    std::cout << "SORT TIME: " << sort_time << std::endl;
    prev_arr.resize(requestcopy.size() + 1);

    std::vector<req_index_pair> living_req;
#pragma omp parallel for
    for (uint64_t i = 0; i < requestcopy.size(); i++) {
      auto [addr, access_num] = requestcopy[i];
      auto [last_addr, last_access_num] = i == 0 ? req_index_pair(0, 0): requestcopy[i-1];

      // Using last, check if previous sorted access is the same
      if (last_access_num > 0 && addr == last_addr) {
        prev(access_num) = last_access_num;  // Point access to previous
      } else {
        prev(access_num) = 0;  // last is different so prev = 0
        if (calc_living && i > 0) { // add last to living requests
          #pragma omp critical
          {
            living_req.push_back({last_access_num, last_addr});
          }
        }
      }
    }

    auto [last_addr, last_access_num] = requestcopy[requestcopy.size()-1];
    living_req.push_back({last_access_num, last_addr});

    if (!calc_living)
      return living_req;

    // sort by access number and then reverse to get living
    // requests sorted by access number
    std::sort(living_req.begin(), living_req.end());
    std::vector<req_index_pair> new_living(living_req.size());

#pragma omp parallel for
    for (uint64_t i = 0; i < living_req.size(); i++) {
      new_living[i].first = living_req[i].second;
      new_living[i].second = living_req[i].first;
    }
    return new_living;
  }

  std::vector<uint64_t> IncrementAndKill::get_distance_vector() {
    std::vector<uint64_t> distance_vector(requests.size()+1);

    // Generate the list of operations
    // Here, we init enough space for all operations.
    // Every kill is either a kill or not
    // Every subrange increment can expand into at most 2 non-passive ops
    std::cout << "MIP Requesting memory: " << sizeof(Op) * 2 * 2 * requests.size() * 1.0 / GB << " GB" << std::endl;
    std::vector<Op> operations(2*requests.size());
    std::vector<Op> scratch(2*requests.size());

// Increment(prev(i)+1, i-1, 1)
// Kill(prev(i))

// We encode the above with:

// Prefix Inc i-1, 1
// Full increment -1
// Kill prev(i)
// Suffix Increment prev(i)

// Note: 0 index vs 1 index
    for (uint64_t i = 0; i < requests.size(); i++) {

      operations[2*i] = Op(i, -1); // Prefix i, +1, Full -1        
      operations[2*i+1] = Op(prev(i+1));  
    }

    // begin the recursive process
    //TODO: add a constructor for this????
    ProjSequence init_seq(1, requests.size());
    init_seq.op_seq = operations.begin();
    init_seq.scratch = scratch.begin();
    init_seq.num_ops = operations.size();
    init_seq.len = operations.size();

    // We want to spin up a bunch of threads, but only start with 1.
    // More will be added in by do_projections.
#pragma omp parallel
#pragma omp single
    do_projections(distance_vector, std::move(init_seq));

    return distance_vector;
  }

  IAKOutput IncrementAndKill::get_depth_vector(std::vector<req_index_pair> &living_requests, std::vector<req_index_pair> &chunk) {
    // TODO: be less dumb than this
    IAKOutput ret;
    requests.clear();
    requests.reserve(living_requests.size() + chunk.size());
    requests.insert(requests.end(), living_requests.begin(), living_requests.end());
    requests.insert(requests.end(), chunk.begin(), chunk.end());

    ret.living_requests = calculate_prevnext(true);
    ret.depth_vector.resize(living_requests.size() + chunk.size() + 1);

    // Generate the list of operations
    // Here, we init enough space for all operations.
    // Every kill is either a kill or not
    // Every subrange increment can expand into at most 2 non-passive ops
    size_t arr_size = 2 * (living_requests.size() + chunk.size());
    std::cout << "MIP Requesting memory: " << sizeof(Op) * 2 * arr_size * 1.0 / GB << " GB" << std::endl;
    std::vector<Op> operations(arr_size);
    std::vector<Op> scratch(arr_size);

    // Increment(prev(i)+1, i-1, 1)
    // Kill(prev(i))

    // We encode the above with:

    // Prefix Inc i-1, 1
    // Full increment -1
    // Kill prev(i)
    // Suffix Increment prev(i)

    // Null requests to give space for indices where living requests reside

    // Note: 0 index vs 1 index
    for (uint64_t i = living_requests.size(); i < chunk.size() + living_requests.size(); i++) {
      auto[request_id, request_index] = chunk[i - living_requests.size()];
      operations[2*i] = Op(request_index, -1); // Prefix i, +1, Full -1
      operations[2*i+1] = Op(prev(request_index));
    }

    size_t max_index = chunk[chunk.size() - 1].second;

    // begin the recursive process
    //TODO: add a constructor for this????
    ProjSequence init_seq(1, max_index);
    init_seq.op_seq  = operations.begin();
    init_seq.scratch = scratch.begin();
    init_seq.num_ops = operations.size();
    init_seq.len     = operations.size();

    // We want to spin up a bunch of threads, but only start with 1.
    // More will be added in by do_projections.
#pragma omp parallel
#pragma omp single
    do_projections(ret.depth_vector, std::move(init_seq));

    return ret;
  }

  //recursively (and in parallel) perform all the projections
  void IncrementAndKill::do_projections(std::vector<uint64_t>& distance_vector, ProjSequence cur)
  {
    // base case
    // start == end -> d_i [operations]
    // operations = [Inc, Kill], [Kill, Inc]
    // if Kill, Inc, then distance = 0 -> sequence = [... p_x, p_x ...]
    // No need to lock here-- this can only occur in exactly one thread
    if (cur.num_ops == 0)
      return;
    if (cur.start == cur.end) {
      // There is always an op
      distance_vector[cur.start] = cur.op_seq[0].get_full_amnt();
      // There is sometimes a postfix (kill)
      // We have to add the kill's increment amount
      if (cur.num_ops > 1) {
        distance_vector[cur.start] += cur.op_seq[1].get_inc_amnt();
      }
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

  std::vector<uint64_t> IncrementAndKill::get_success_function() {
    calculate_prevnext();
    auto distances = get_distance_vector();

    // a point representation of successes
    std::vector<uint64_t> success(distances.size());
    std::cout << "distance vector: " << distances.size() << std::endl;
    for (uint64_t i = 1; i < distances.size()-1; i++) {
      std::cout << distances[i] << " ";
      if (prev(i + 1) != 0) success[distances[prev(i + 1)]]++;
    }
    std::cout << std::endl;

    // integrate
    uint64_t running_count = 0;
    for (uint64_t i = 1; i < success.size(); i++) {
      running_count += success[i];
      success[i] = running_count;
    }
    return success;
  }

  // Create a new Operation by projecting another
  Op::Op(const Op& oth_op, uint64_t proj_start, uint64_t proj_end){
    _target = oth_op._target;
    full_amnt = oth_op.full_amnt;
    switch(oth_op.type())
    {
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
}
