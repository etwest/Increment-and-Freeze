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

  // Generate the list of operations
  std::vector<Op> operations;
  for (uint64_t i = 0; i < requests.size(); i++) {
    operations.push_back(Op(prev(i)+1, i-1)); // Increment(prev(i)+1, i-1, 1)
    operations.push_back(Op(prev(i)));        // Kill(prev(i))
  }

  // begin the 'recursive' process
  std::stack<proj_sequence> rec_stack;
  proj_sequence init_seq;
  init_seq.op_seq = operations;
  init_seq.start  = 0;
  init_seq.end    = requests.size() - 1;
  rec_stack.push(init_seq);

  while (!rec_stack.empty()) {
    proj_sequence cur = rec_stack.top();
    rec_stack.pop();

    // base case
    if (cur.start == cur.end) {

    }
    // recursive case
    else {
      uint64_t mid = (cur.end - cur.start) / 2 + cur.start;
      
      // generate projected sequence for first half
      proj_sequence fst_half;
      fst_half.start = cur.start;
      fst_half.end = mid;
      for (uint64_t i = 0; i < cur.op_seq.size(); i++) {
        Op proj_op = Op(cur.op_seq[i], fst_half.start, fst_half.end);
        fst_half.op_seq.push_back(proj_op);
      }
      rec_stack.push(fst_half);

      // generate projected sequence for second half
      proj_sequence snd_half;
      snd_half.start = mid + 1;
      snd_half.end = cur.end;
      for (uint64_t i = 0; i < cur.op_seq.size(); i++) {
        Op proj_op = Op(cur.op_seq[i], snd_half.start, snd_half.end);
        snd_half.op_seq.push_back(proj_op);
      }
      rec_stack.push(snd_half);
    }
  }

  return std::vector<uint64_t>();
}

// Create a new Operation by projecting another
Op::Op(Op oth_op, uint64_t proj_start, uint64_t proj_end) {
  // check if Op becomes Null
  if ((oth_op.type == Increment && oth_op.end < proj_start) ||
      oth_op.start > proj_end) {
    type = Null;
    return;
  }

  // kills do not shrink
  if (oth_op.type == Kill) {
    type = Kill;
    start = oth_op.start;
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
