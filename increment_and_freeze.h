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
// Prefix and Postfix are encoded by a single bit at the beginning of _target
// Null is encoded by an entirely zero _target variable
enum OpType {Prefix=0, Postfix=1, Null=2};

// An IAF operation
class Op {
 private:
  uint32_t _target = 0;                   // Boundary of operation
  static constexpr uint32_t inc_amnt = 1; // subrange Increment amount
  int32_t full_amnt = 0;                  // fullrange Increment amount

  static constexpr uint32_t tmask = 0x7FFFFFFF;
  static constexpr uint32_t ntmask = ~tmask;
  uint32_t target() const {return _target & tmask;};
  void set_target(const uint32_t& new_target) {
    assert(new_target == (new_target & tmask));
    _target &= ntmask;
    _target |= new_target;
  };
  inline void set_type(const OpType& t) {
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

  friend std::ostream& operator<<(std::ostream& os, const Op& op) {
    switch (op.get_type()) {
      case Prefix:  os << "Po:" << op.target() << "-Inf" << ".+" << op.full_amnt; break;
      case Postfix: os << "Pr:0-" << op.target() << ".+" << op.full_amnt; break;
      case Null:    os << "N:" << "+" << op.full_amnt; break;
      default: std::cerr << "ERROR: Unrecognized op.get_type()" << std::endl; break;
    }
    return os;
  }

  inline void make_null() { _target = 0; }
  inline void add_full(size_t oth_full_amnt) { full_amnt += oth_full_amnt; }

  // returns if this operation will cross from right to left
  inline bool move_to_scratch(uint64_t proj_start) const {
    return target() < proj_start && get_type() == Postfix;
  }

  // returns if this operation is the boundary prefix op
  // boundary operations target the end of the left partition and are Prefixes
  inline bool is_boundary_op(uint64_t left_end) const {
    return target() == left_end && get_type() == Prefix;
  }

  inline size_t get_full_incr_to_left(uint64_t right_start) const {
    // if a Prefix and target is in right then both full and inc affect
    // left side as a full
    if (get_type() == Prefix && target() >= right_start)
      return get_inc_amnt() + get_full_amnt();

    // otherwise only full amount counts
    return get_full_amnt();
  }

  inline OpType get_type() const {
    if (is_null()) return Null;
    else return (OpType)(_target >> 31);
  }
  inline bool is_null() const          { return _target == 0; }
  inline uint64_t get_target() const   { return target(); }
  inline uint64_t get_inc_amnt() const { return inc_amnt; }
  inline int64_t get_full_amnt() const { return full_amnt; }
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
    // std::cout << *this << std::endl;
    // std::cout << "Partitioning into: " << left.start << "-" << left.end << ", ";
    // std::cout << right.start << "-" << right.end << std::endl;

    assert(left.start <= left.end);
    assert(left.end+1 == right.start);
    assert(right.start <= right.end);
    assert(start == left.start);
    assert(end == right.end);

    std::vector<Op> scratch_stack;

    // Where we merge operations that remain on the right side
    // use ints for this and cur_idx because underflow is good and tells us things
    int merge_into_idx = num_ops - 1;

    // loop through all the operations on the right side
    int cur_idx;
    size_t full_incr_to_left = 0; // amount of full increments to left created by prefix with target in right
    for (cur_idx = num_ops - 1; cur_idx >= 0; cur_idx--) {
      Op& op = op_seq[cur_idx];

      if (op.is_boundary_op(left.end)) {
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
        --cur_idx;
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
          op = Op(); // set where op used to be to a no_impact operation
        }
        // if moved operation is not null then we need to decr merge_into_idx
        if (!op_seq[merge_into_idx].is_null()) merge_into_idx--;
      }

      // // Print out operations
      // std::cout << "cur_idx = " << cur_idx - 1 << " merge_into_idx = " << merge_into_idx << std::endl;
      // std::cout << "full_incr_to_left = " << full_incr_to_left << std::endl;
      // std::cout << *this << std::endl;

      // // print out scratch stack
      // std::cout << "Scratch_stack: " << std::endl;
      // for (auto op : scratch_stack)
      //   std::cout << op << " ";
      // std::cout << std::endl << std::endl;
    }

    // // Print out operations
    // std::cout << "Done processing projection" << std::endl;
    // std::cout << "cur_idx = " << cur_idx << " merge_into_idx = " << merge_into_idx << std::endl;
    // std::cout << *this << std::endl;

    // // print out scratch stack
    // std::cout << "Scratch_stack: " << std::endl;
    // for (auto op : scratch_stack)
    //   std::cout << op << " ";
    // std::cout << std::endl << std::endl;

    
    if (cur_idx >= 0) {
      // update last op on left with the full increment from the right
      op_seq[cur_idx].add_full(full_incr_to_left);
    }
    
    // merge_into_idx belongs to right partition
    // [cur_idx+1, merge_into_idx) belongs to left partition (where scratch_stack goes)
    assert(merge_into_idx - cur_idx - 1 >= (int) scratch_stack.size());
    if (scratch_stack.size() > 0) {
      for (int i = scratch_stack.size() - 1; i >= 0; i--)
        op_seq[++cur_idx] = scratch_stack[i];
    }


    // This fails if there isn't enough memory allocated
    // It either means we did something wrong, or our memory
    // bound is incorrect.

    //Now op_seq and scratch are properly named. We assign them to left and right.
    left.op_seq  = op_seq;
    left.num_ops = cur_idx + 1;

    right.op_seq = op_seq + merge_into_idx;
    right.num_ops = num_ops - merge_into_idx;
    //std::cout /*<< total*/ << "(" << len << ") " << " -> " << left.len << ", " << right.len << std::endl;
  
    // Print out final projection
    // std::cout << "Final Result: " << std::endl;
    // std::cout << "LEFT:  " << left << std::endl;
    // std::cout << "RIGHT: " << right << std::endl << std::endl;
  }

  friend std::ostream& operator<<(std::ostream& os, const ProjSequence& seq) {
    os << "start = " << seq.start << " end = " << seq.end << std::endl;
    os << "num_ops = " << seq.num_ops << std::endl;
    os << "Operations: ";
    for (size_t i = 0; i < seq.num_ops; i++)
      os << seq.op_seq[i] << " ";
    return os;
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

  /* Vector of operations used in ProjSequence to store memory operations
   */
  std::vector<Op> operations;

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
