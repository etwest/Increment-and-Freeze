#include "IncrementAndKillSmallInPlace.h"

#include <algorithm>
#include <queue>
#include <omp.h>
#include "params.h"
class IncrementAndKill;

namespace SmallInPlace {

  void IncrementAndKill::memory_access(uint64_t addr) {
    requests.push_back({addr, access_number++});
  }

  void IncrementAndKill::calculate_prevnext() {
    // put all requests of the same addr next to each other
    // then order those by access_number
    auto requestcopy = requests;

    std::sort(requestcopy.begin(), requestcopy.end());

    prevnext.resize(requestcopy.size() + 1);

#pragma omp parallel for
    for (uint64_t i = 0; i < requestcopy.size(); i++) {
      auto [addr, access_num] = requestcopy[i];
      auto [last_addr, last_access_num] = i == 0 ? tuple(0, 0): requestcopy[i-1];

      // Using last, check if previous sorted access is the same
      if (last_access_num > 0 && addr == last_addr) {
        prev(access_num) = last_access_num;  // Point access to previous
        next(last_access_num) = access_num;  // previous access to this
      } else {
        prev(access_num) = 0;  // last is different so prev = 0
      }

      // Preemptively point this one's next access to the end
      next(access_num) = requestcopy.size() + 1;
    }
  }

  std::vector<uint64_t> IncrementAndKill::get_distance_vector() {
    std::vector<uint64_t> distance_vector(requests.size()+1);

    // Generate the list of operations
    // Here, we init enough space for all operations.
    // Every kill is either a kill or not
    // Every subrange increment can expand into at most 2 non-passive ops
    std::cout << "Requesting memory: " << sizeof(Op) * 4 * 2 * requests.size() * 1.0 / GB << std::endl;
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
    ProjSequence init_seq(1, requests.size());
    init_seq.op_seq = operations.begin();
    init_seq.scratch = scratch.begin();
    init_seq.len = operations.size();

    // We want to spin up a bunch of threads, but only start with 1.
    // More will be added in by do_projections.
#pragma omp parallel
#pragma omp single
    do_projections(distance_vector, std::move(init_seq));

    return distance_vector;
  }

  //recursively (and in parallel) perform all the projections
  void IncrementAndKill::do_projections(std::vector<uint64_t>& distance_vector, ProjSequence cur)
  {
    // base case
    // start == end -> d_i [operations]
    // operations = [Inc, Kill], [Kill, Inc]
    // if Kill, Inc, then distance = 0 -> sequence = [... p_x, p_x ...]
    // No need to lock here-- this can only occur in exactly one thread
    if (cur.len == 0)
      return;
    if (cur.start == cur.end) {
      if (cur.len > 0)
        distance_vector[cur.start] = cur.op_seq[0].get_full_amnt();
      else
        distance_vector[cur.start] = 0;
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
    for (uint64_t i = 1; i < distances.size()-1; i++) {
      if (prev(i + 1) != 0) success[distances[prev(i + 1)] + 1]++;
    }
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
		target = oth_op.target;
    full_amnt = oth_op.full_amnt;
    switch(oth_op.type)
    {
      case Prefix: //we affect before target
        if (proj_start > target)
        {
          type = Null;
        }
        else if (proj_end >= target) //full inc case
        {
          type = Null;
          full_amnt += oth_op.inc_amnt;
        }
        else // prefix case
        {
          type = Prefix;
        }
        return;
      case Postfix: //we affect after target
        if (proj_end < target)
        {
          type = Null;
        }
        else if (proj_start <= target) //full inc case
        {
          type = Null;
          full_amnt += oth_op.inc_amnt;
        }
        else // postfix case
        {
          type = Postfix;
        }
        return;
      case Null:
        type = Null;
        return;
      default: assert(false); return;
    }
  }
}
