#ifndef ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
#define ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>    // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>     // for pair, move, swap
#include <vector>      // for vector, vector<>::iterator

#include "cache_sim.h"  // for CacheSim


struct IAKOutput {
  std::vector<std::pair<size_t, size_t>> living_requests;
  std::vector<size_t> depth_vector;
};

struct IAKInput {
  IAKOutput output;                                      // output of the previous chunk
  std::vector<std::pair<size_t, size_t>> chunk_requests; // living requests and fresh requests
};

// Operation types and be Prefix, Postfix, or Null
// OpType is used to encode Prefix and Postfix
// Null is encoded by an entirely zero _target variable
enum OpType {Prefix=0, Postfix=1};

// A single operation, such as increment or kill
class Op {
 private:
  //OpType type = Null;     // Do we increment or kill?
  uint32_t _target = 0; // kill target
  static constexpr uint32_t inc_amnt = 1;      // subrange Increment amount
  int32_t full_amnt = 0;      // fullrange Increment amount

  static constexpr uint32_t tmask = 0x7FFFFFFF;
  static constexpr uint32_t ntmask = ~tmask;
  uint32_t target() const {return _target & tmask;};
  void set_target(const uint32_t& new_target) {
    assert(new_target == (new_target & tmask));
    _target &= ntmask;
    _target |= new_target;
  };
  
  OpType type() const { return (OpType)(_target >> 31); };
  void set_type(const OpType& t) {
    _target &= tmask;
    _target |= ((int)t << 31);
  };
 public:
  // create an Prefix (if target is 0 -> becomes a Null op)
  Op(uint64_t target, int64_t full_amnt)
      : full_amnt(full_amnt){set_type(Prefix); set_target(target);};

  // create a Postfix
  Op(uint64_t target){set_type(Postfix); set_target(target);};

  // Uninitialized. Used to parallelize making a vector of this without push_back
  Op() : _target(0) {};

  // create a new Op by projecting another one
  // Op(const Op& oth_op, uint64_t proj_start, uint64_t proj_end);

  // This enforces correct merging of operators
  // Can only be done on equal subrange
  /*Op& operator+=(Op& oth) {
    assert(oth.type == Subrange);
    assert(type == Subrange);
    assert(start == oth.start);
    assert(end == oth.end);

    inc_amnt += oth.inc_amnt;
    full_amnt += oth.full_amnt;
    return *this;
    }*/
  bool affects(size_t victum) const {
    return (type() == Prefix && victum <= target()) || (type() == Postfix && victum >= target());
  }

  friend std::ostream& operator<<(std::ostream& os, const Op& op) {
    if (op.is_null())
      os << "N:" << "+" << op.full_amnt;
    else if (op.get_type() == Prefix)
      os << "Pr:0-" << op.target() << ".+" << op.full_amnt;
    else
      os << "Po:" << op.target() << "-Inf" << ".+" << op.full_amnt;
    return os;
  }

  void make_null() { _target = 0; }

  void add_full(size_t oth_full_amnt) {
    full_amnt += oth_full_amnt;
  }

  // return if this operation has no impact
  bool no_impact() const {
    return (get_full_amnt() == 0 && is_null());
  }

  // is this operation passive in the projection defined by
  // proj_start and proj_end
  bool is_passive(uint64_t proj_start, uint64_t proj_end) const {
    return !affects(proj_start) && !affects(proj_end);
  }

  // returns if this operation will cross from right to left
  bool move_to_scratch(uint64_t proj_start) const {
    return target() < proj_start && type() == Postfix;
  }

  // returns if this operation is the leftmost op on right side
  bool boundary_op(uint64_t proj_start) const {
    return target() < proj_start && type() == Prefix && !is_null();
  }

  size_t get_full_incr_to_left(uint64_t proj_start) const {
    if (type() == Prefix && target() >= proj_start)
      return get_inc_amnt() + get_full_amnt();
    else if (type() == Postfix) {
      return get_full_amnt();
    }
    return 0;
  }

  OpType get_type() const       { return type(); }
  bool is_null() const          { return _target == 0; }
  uint64_t get_target() const   { return target(); }
  uint64_t get_inc_amnt() const { return inc_amnt; }
  int64_t get_full_amnt() const { return full_amnt; }
};

// A sequence of operators defined by a projection
class ProjSequence {
 public:
  std::vector<Op>::iterator op_seq; // iterator to beginning of operations sequence
  size_t num_ops;                   // number of operations in this projection

  // Request sequence range
  uint64_t start;
  uint64_t end;

  // Initialize an empty projection with bounds (to be filled in by partition)
  ProjSequence(uint64_t start, uint64_t end) : start(start), end(end) {};

  // Init a projection with bounds and iterators
  ProjSequence(uint64_t start, uint64_t end, std::vector<Op>::iterator op_seq, size_t num_ops) : op_seq(op_seq), num_ops(num_ops), start(start), end(end) {};
  
  void partition(ProjSequence& left, ProjSequence& right) {
    // std::cout << "Performing partition upon projected sequence" << std::endl;
    // std::cout << "num_ops = " << num_ops << std::endl;
    // std::cout << "left.start  = " << left.start << std::endl;
    // std::cout << "left.end    = " << left.end << std::endl;
    // std::cout << "right.start = " << right.start << std::endl;
    // std::cout << "right.end   = " << right.end << std::endl;

    assert(left.start <= left.end);
    assert(left.end+1 == right.start);
    assert(right.start <= right.end);
    assert(start == left.start);
    assert(end == right.end);

    std::vector<Op> scratch_stack;

    // Where we merge operations that remain on the right side
    size_t merge_into_idx = num_ops - 1;

    // loop through all the operations on the right side
    size_t cur_idx;
    size_t full_incr_to_left = 0; // amount of full increments to left created by prefix with target in right
    for (cur_idx = num_ops - 1; cur_idx >= 0; cur_idx--) {
      Op& op = op_seq[cur_idx];

      if (op.boundary_op(right.start)) {
        // we merge this op with the next left op (also need to add inc amount to full)
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
        break;
      }

      if (op.move_to_scratch(right.start)) {
        scratch_stack.push_back(op);    // copy this op into scratch_stack
        scratch_stack[scratch_stack.size() - 1].add_full(full_incr_to_left);
        full_incr_to_left = 0;

        if (cur_idx != merge_into_idx) {
          // merge this operation with merge_into_idx and make this op no impact
          op_seq[merge_into_idx].add_full(op.get_full_amnt() + op.get_inc_amnt());
          op = Op();
        } else {
          // make this operation null and add incr amount to full
          op.make_null();
          op.add_full(op.get_inc_amnt());
        }
      }
      else {
        // we don't move this op to left but does it affect the full incr of the left
        full_incr_to_left += op.get_full_incr_to_left(right.start);

        if (merge_into_idx != cur_idx) {
          // merge current op into merge idx op
          size_t full = op_seq[merge_into_idx].get_full_amnt();
          op.add_full(full);
          op_seq[merge_into_idx] = op;
          op = Op(); // set where op used to be to a no_impact() operation
          assert(op.no_impact());
        }
        merge_into_idx--;
      }

      // Print out operations
      // std::cout << "cur_idx = " << cur_idx - 1 << " merge_into_idx = " << merge_into_idx << std::endl;
      // std::cout << "full_incr_to_left = " << full_incr_to_left << std::endl;
      // std::cout << "Operations: " << std::endl;
      // for (size_t i = 0; i < num_ops; i++)
      //   std::cout << op_seq[i] << " ";
      // std::cout << std::endl << std::endl;

      // // print out scratch stack
      // std::cout << "Scratch_stack: " << std::endl;
      // for (auto op : scratch_stack)
      //   std::cout << op << " ";
      // std::cout << std::endl << std::endl;
    }

    // Print out operations
    // std::cout << "Done processing projection" << std::endl;
    // std::cout << "cur_idx = " << cur_idx << " merge_into_idx = " << merge_into_idx << std::endl;
    // std::cout << "Operations: " << std::endl;
    // for (size_t i = 0; i < num_ops; i++)
    //   std::cout << op_seq[i] << " ";
    // std::cout << std::endl << std::endl;

    // // print out scratch stack
    // std::cout << "Scratch_stack: " << std::endl;
    // for (auto op : scratch_stack)
    //   std::cout << op << " ";
    // std::cout << std::endl << std::endl;

    // update last op on left with the full increment from the right
    op_seq[cur_idx - 1].add_full(full_incr_to_left);

    // The right side cares about the full amount in the merge_into_idx
    // merge_into_idx belongs to right partition
    assert(merge_into_idx - cur_idx >= scratch_stack.size());
    if (scratch_stack.size() > 0) {
      for (int i = scratch_stack.size() - 1; i >= 0; i--)
        op_seq[cur_idx++] = scratch_stack[i];
    }
    

    // std::cout << "adding stack space done!" << std::endl;

    // This fails if there isn't enough memory allocated
    // It either means we did something wrong, or our memory
    // bound is incorrect.

    //Now op_seq and scratch are properly named. We assign them to left and right.
    left.op_seq  = op_seq;
    left.num_ops = cur_idx;

    right.op_seq = op_seq + merge_into_idx;
    right.num_ops = num_ops - merge_into_idx;
    //std::cout /*<< total*/ << "(" << len << ") " << " -> " << left.len << ", " << right.len << std::endl;
  
    // Print out final projection
    // std::cout << "Done merging in scratch" << std::endl;
    // std::cout << "left.num_ops = " << left.num_ops << std::endl;
    // std::cout << "Left operations: " << std::endl;
    // for (size_t i = 0; i < left.num_ops; i++)
    //   std::cout << left.op_seq[i] << " ";
    // std::cout << std::endl;

    // std::cout << "right.num_ops = " << right.num_ops << std::endl;
    // std::cout << "Right operations: " << std::endl;
    // for (size_t i = 0; i < right.num_ops; i++)
    //   std::cout << right.op_seq[i] << " ";
    // std::cout << std::endl << std::endl;
  }
};

// Implements the IncrementAndFreezeInPlace algorithm
class IncrementAndFreeze: public CacheSim {
 public:
  using req_index_pair = std::pair<uint64_t, uint64_t>;
 private:
  // A vector of all requests
  std::vector<req_index_pair> requests;

  /* A vector of tuples for previous and next
   * Previous defines the last instance of a page
   * next defines the next instance of a page
   */
  std::vector<size_t> prev_arr;

  /* This converts the requests into the previous and next vectors
   * Requests is copied, not modified.
   * Precondition: requests must be properly populated.
   */
  void calculate_prevnext(std::vector<req_index_pair> &req,
                          std::vector<req_index_pair> *living_req=nullptr);

  /* Vector of operations used in ProjSequence to store memory
   * operations and scratch are constantly swapped to allow to a pseudo-in place partition
   */
  std::vector<Op> operations;
  std::vector<Op> scratch;

  /* Returns the distance vector calculated from prevnext.
   * Precondition: prevnext must be properly populated.
   */
  //std::vector<uint64_t> get_distance_vector();
  // Shortcut to access prev in prevnext.
  uint64_t& prev(uint64_t i) {return prev_arr[i];}
  /* Helper dunction to get_distance_vector.
   * Recursively (and in parallel) populates the distance vector if the
   * projection is small enough, or calls itself with smaller projections otherwise.
   */
  void do_projections(std::vector<uint64_t>& distance_vector, ProjSequence seq);
 public:
  // Logs a memory access to simulate. The order this function is called in matters.
  void memory_access(uint64_t addr);
  /* Returns the success function.
   * Does *a lot* of work.
   * When calling print_success_function, the answer is re-computed.
   */
  std::vector<uint64_t> get_success_function();

  /*
   * Process a chunk of requests using the living requests from the previous chunk
   * Return the new living requests and the depth_vector
   */
  void get_depth_vector(IAKInput &chunk_input);

  IncrementAndFreeze() = default;
  ~IncrementAndFreeze() = default;
  std::vector<uint64_t> get_distance_vector();
};

#endif  // ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
