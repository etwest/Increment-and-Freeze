#include "IncrementAndKillInPlace.h"

#include <algorithm>
#include <queue>
#include <omp.h>

void IncrementAndKillInPlace::memory_access(uint64_t addr) {
  requests.push_back({addr, access_number++});
}

void IncrementAndKillInPlace::calculate_prevnext() {
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

std::vector<uint64_t> IncrementAndKillInPlace::get_distance_vector() {
  std::vector<uint64_t> distance_vector(requests.size() + 1);

  // Generate the list of operations
	// Here, we init enough space for all operations.
	// Every kill is either a kill or not
	// Every subrange increment can expand into at most 2 non-passive ops
  std::vector<ipOp> operations(3*requests.size());
  std::vector<ipOp> scratch(3*requests.size());

  for (uint64_t i = 0; i < requests.size(); i++) {
	
    operations[3*i] = ipOp(prev(i+1) + 1, i);        // Increment(prev(i)+1, i-1, 1)
    operations[3*i+1] = ipOp(prev(i+1));  // Kill(prev(i))
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
void IncrementAndKillInPlace::do_projections(std::vector<uint64_t>& distance_vector, ProjSequence cur)
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

std::vector<uint64_t> IncrementAndKillInPlace::get_success_function() {
  calculate_prevnext();
  auto distances = get_distance_vector();

  // a point representation of successes
  std::vector<uint64_t> success(distances.size());
  for (uint64_t i = 1; i < distances.size(); i++) {
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

// Create a new ipOperation by projecting another
ipOp::ipOp(const ipOp& oth_op, uint64_t proj_start, uint64_t proj_end){
  full_amnt = oth_op.full_amnt;
  switch(oth_op.type)
  {
    case Subrange:
    case Prefix:
    case Postfix:
      start = oth_op.start < proj_start ? proj_start : oth_op.start;
      end = oth_op.end > proj_end ? proj_end : oth_op.end;
      if (end < start)
      {
        type = Null;
        full_amnt = 0; // invalid case
        inc_amnt = 0;
      }
      else if (start == proj_start && end == proj_end) //full inc case
      {
        type = Null;
        full_amnt += oth_op.inc_amnt;
        inc_amnt = 0;
      }
      else if (start == proj_start) // prefix case
      {
        type = Prefix;
        inc_amnt = oth_op.inc_amnt;
      }
      else if (end == proj_end) //postfix case
      {
        type = Postfix;
        inc_amnt = oth_op.inc_amnt;
      }
      else //subrange case
      {
        assert(oth_op.type == Subrange);
        type = Subrange;
        inc_amnt = oth_op.inc_amnt;
      }
      return;
    case Null:
      type = Null;
      inc_amnt = 0;
      return;
    case Kill:
      target = oth_op.target;
      if (proj_start <= target && target <= proj_end)
        type = Kill;
      else
        type = Null;
      return;
    default: assert(false); return;
  }
}

