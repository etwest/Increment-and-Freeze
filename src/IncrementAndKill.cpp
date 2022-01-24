#include "IncrementAndKill.h"

#include <algorithm>
#include <stack>

void IncrementAndKill::memory_access(uint64_t addr) {
  requests.push_back({addr, access_number++});
}

void IncrementAndKill::calculate_prevnext() {
  // put all requests of the same addr next to each other
  // then order those by access_number
  std::sort(requests.begin(), requests.end());

  prevnext.resize(requests.size() + 1);

  tuple last; // i-1th element
  for (int i = 0; i < requests.size(); i++) {
    auto [addr, access_num] = requests[i];
    auto [last_addr, last_access_num] = last;

    // Using last, check if previous sorted access is the same
    if (last_access_num > 0 && addr == last_addr) {
      prev(access_num) = last_access_num; // Point access to previous
      next(last_access_num) = access_num; // previous access to this
    } 
    else {
      prev(access_num) = 0; // last is different so prev = 0
    }

    // Preemptively point this one's next access to the end
    next(access_num) = requests.size() + 1;
    last = requests[i];
  }
}

std::vector<uint64_t> IncrementAndKill::get_success_function() {
  calculate_prevnext();
  std::vector<uint64_t> distance_vector;
  distance_vector.reserve(requests.size());

  // Generate the list of operations
  std::vector<Op> operations;
  operations.reserve(2 * requests.size());
  for (uint64_t i = 1; i <= requests.size(); i++) {
    operations.push_back(Op(prev(i)+1, i-1)); // Increment(prev(i)+1, i-1, 1)
    operations.push_back(Op(prev(i)));        // Kill(prev(i))
  }

  // begin the 'recursive' process
  std::stack<ProjSequence> rec_stack;
  ProjSequence init_seq(0, requests.size() - 1);
  init_seq.op_seq = operations;
  rec_stack.push(init_seq);

  while (!rec_stack.empty()) {
    ProjSequence cur = rec_stack.top();
    rec_stack.pop();

    // base case
    // start == end -> d_i [operations]
    // operations = [Inc, Kill], [Kill, Inc]
    // if Kill, Inc, then distance = 0 -> sequence = [... p_x, p_x ...]
    if (cur.start == cur.end) {
      if (cur.op_seq.size() > 0 && cur.op_seq[0].get_type() == Increment)
        distance_vector.push_back(cur.op_seq[0].get_r());
      else
        distance_vector.push_back(0);
    }
    // recursive case
    else {
      uint64_t mid = (cur.end - cur.start) / 2 + cur.start;
      
      // generate projected sequence for first half
      ProjSequence fst_half(cur.start, mid);
      for (uint64_t i = 0; i < cur.op_seq.size(); i++) {
        fst_half.add_op(cur.op_seq[i]);
      }
      rec_stack.push(fst_half);

      // generate projected sequence for second half
      ProjSequence snd_half(mid + 1, cur.end);
      for (uint64_t i = 0; i < cur.op_seq.size(); i++) {
        snd_half.add_op(cur.op_seq[i]);
      }
      rec_stack.push(snd_half);
    }
  }

  for (uint64_t i = 0; i < distance_vector.size(); i++) {
    std::cout << i << ": " << requests[i].first << " -- " << distance_vector[i] << std::endl;
  }

  return std::vector<uint64_t>();
}

// Create a new Operation by projecting another
Op::Op(Op oth_op, uint64_t proj_start, uint64_t proj_end) {
  // check if Op becomes Null
  // Increments are Null if we end before the start OR have a 'bad' interval (end before our start)
  if (oth_op.type == Increment && (oth_op.end < oth_op.start || oth_op.end < proj_start || oth_op.start > proj_end)) {
    type = Null;
    return;
  }

  // kills do not shrink unless out of bounds
  if (oth_op.type == Kill) {
    if (oth_op.start < proj_start || oth_op.start > proj_end) {
      type = Null;
    }
    else{
      type = Kill;
      start = oth_op.start;
    }
    return;
  }

  // shrink the operation by the projection
  uint64_t n_start = oth_op.start;
  uint64_t n_end = oth_op.end;
  if (proj_start > n_start) n_start = proj_start;
  if (proj_end < n_end) n_end = proj_end;

  type = Increment;
  start = n_start;
  end = n_end;
  r = oth_op.r;
}
