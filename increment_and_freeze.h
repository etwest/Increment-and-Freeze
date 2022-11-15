#ifndef ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
#define ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>    // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>     // for pair, move, swap
#include <vector>      // for vector, vector<>::iterator

#include "cache_sim.h"  // for CacheSim

// Operation types and be Prefix, Postfix, or Null
// Prefix and Postfix are encoded by a single bit at the beginning of _target
// Null is encoded by an entirely zero _target variable
enum OpType {Prefix=0, Postfix=1, Null=2};

static constexpr size_t branching_factor = 4;

// An IAF operation
class Op {
 private:
  uint32_t _target = 0;                   // Boundary of operation
  static constexpr uint32_t inc_amnt = 1; // subrange Increment amount
  int32_t full_amnt = 0;                  // fullrange Increment amount

  static constexpr uint32_t tmask = 0x7FFFFFFF;
  static constexpr uint32_t ntmask = ~tmask;
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
      case Prefix: os << "Pr:0-" << op.get_target() << ".+" << op.full_amnt; break;
      case Postfix:  os << "Po:" << op.get_target() << "-Inf" << ".+" << op.full_amnt; break;
      case Null:    os << "N:" << "+" << op.full_amnt; break;
      default: std::cerr << "ERROR: Unrecognized op.get_type()" << std::endl; break;
    }
    return os;
  }

  inline void make_null() { _target = 0; }
  inline void add_full(size_t oth_full_amnt) { full_amnt += oth_full_amnt; }

  // returns if this operation will cross from right to left
  inline bool move_to_scratch(uint64_t proj_start) const {
    return get_target() < proj_start && get_type() == Postfix;
  }

  // returns if this operation is the boundary prefix op
  // boundary operations target the end of the left partition and are Prefixes
  inline bool is_boundary_op(uint64_t left_end) const {
    return get_target() == left_end && get_type() == Prefix;
  }

  inline size_t get_full_incr_to_left(uint64_t right_start) const {
    // if a Prefix and target is in right then both full and inc affect
    // left side as a full
    if (get_type() == Prefix && get_target() >= right_start)
      return get_inc_amnt() + get_full_amnt();

    // otherwise only full amount counts
    return get_full_amnt();
  }

  inline OpType get_type() const {
    if (is_null()) return Null;
    else return (OpType)(_target >> 31);
  }
  inline bool is_null() const          { return _target == 0; }
  inline uint64_t get_target() const   { return _target & tmask; }
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

  void partition(ProjSequence& left, ProjSequence& right, size_t split_off_idx, size_t div_factor, size_t orig_length,
                 std::array<std::vector<Op>, branching_factor-1>& partition_scratch_spaces) {
    std::cout << "Performing partition upon projected sequence" << std::endl;
    std::cout << *this << std::endl;
    std::cout << "Partitioning into: " << left.start << "-" << left.end << ", ";
    std::cout << right.start << "-" << right.end << std::endl;
    std::cout << std::endl;

    assert(left.start <= left.end);
    assert(left.end+1 == right.start);
    assert(right.start <= right.end);
    assert(start == left.start);
    assert(end == right.end);
    assert(op_seq[0].is_null());

    // Where we merge operations that remain on the right side
    // use ints for this and cur_idx because underflow is good and tells us things
    int merge_into_idx = num_ops - 1;

    // loop through all the operations on the right side
    int cur_idx;
    for (cur_idx = num_ops - 1; cur_idx >= 0; cur_idx--) {
      Op& op = op_seq[cur_idx];
      
      assert(op.get_type() != Prefix || op.get_target() >= left.end);

      if (op.is_boundary_op(left.end)) {
        std::cout << cur_idx << " is a BOUNDARY OP" << std::endl;
        // we merge this op with the next left op (also need to add inc amount to full)
        // The previous OP is the end of the scratch space
        Op& prev_op = partition_scratch_spaces[split_off_idx-1].front();
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
        std::cout << "MOVING " << op << " left!" << std::endl;
        // TODO: Into which scratch space does this go!
        // NOTE: When a Postfix moves many partitions over its increment and full affect
        // the partitions between its destination and current

        // 1. Identify the partition this Postfix is targeting (inverting the parition map)
        size_t partition_target = (branching_factor-1) - (orig_length - (op.get_target() - (start-1))) / div_factor;
        std::cout << "target in " << partition_target << " / " << split_off_idx << std::endl;
        assert(partition_target < split_off_idx);

        // 2. Place this Postfix in the appropriate scratch space
        std::vector<Op>& scratch_stack = partition_scratch_spaces[partition_target];
        assert(scratch_stack.back().is_null());
        // No increment-- Just total amnt. Save it and overwrite.
        size_t back_total = scratch_stack.back().get_full_amnt();
        scratch_stack.back() = op;
        scratch_stack.back().add_full(back_total);
        scratch_stack.emplace_back();
      

        // 3. For each scratch space in (placement, last) add full+inc to operation at end of that scratch_space
        for (size_t i = partition_target+1; i < split_off_idx; i++) {
          partition_scratch_spaces[i].back().add_full(op.get_full_amnt() + op.get_inc_amnt());
        }
        
        // 4. At this point, we've projected op into [placement, last). Now let's fix the value in last (split_off_idx)
        // Op should be unmodified at this point.
        if (cur_idx != merge_into_idx) {
          // merge this operation with merge_into_idx and make this op no impact
          op_seq[merge_into_idx].add_full(op.get_full_amnt() + op.get_inc_amnt());
          op = Op();
        } else {
          // make this operation null and add incr amount to full
          op.add_full(op.get_inc_amnt());
          op.make_null();
        }
      }
      else {
        // we don't move this op to left but does it affect the full incr of the left
        for (size_t i = 0; i < split_off_idx; i++)  
          partition_scratch_spaces[i].back().add_full(op.get_full_incr_to_left(right.start));

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

      // Print out operations
      std::cout << "cur_idx = " << cur_idx - 1 << " merge_into_idx = " << merge_into_idx << std::endl;
      std::cout << *this << std::endl;

      // print out scratch stack
      std::cout << "Scratch_stack: " << std::endl;
      for (auto &scratch_stack : partition_scratch_spaces) {
        for (auto op : scratch_stack)
          std::cout << op << " ";
        std::cout << std::endl;
      }
      std::cout << std::endl;
    }
    assert(cur_idx >= 0);

    // Print out operations
    std::cout << "Done processing projection" << std::endl;
    std::cout << "cur_idx = " << cur_idx << " merge_into_idx = " << merge_into_idx << std::endl;
    std::cout << *this << std::endl;

    // print out scratch stack
    std::cout << "Scratch_stack: " << std::endl;
    for (auto &scratch_stack : partition_scratch_spaces) {
      for (auto op : scratch_stack)
        std::cout << op << " ";
      std::cout << std::endl;
    }
    std::cout << std::endl;

    // Merge in scratch_stack for relevant partition
    // add full with the null at the end of the scratch stack
    // then iterate from back to add the Postfixes
    std::vector<Op>& scratch_stack = partition_scratch_spaces[split_off_idx - 1];
    assert(scratch_stack.size() > 0 && scratch_stack.back().is_null());
    op_seq[cur_idx].add_full(scratch_stack.back().get_full_amnt());
    scratch_stack.pop_back();

    std::cout << "Scratch_stack at placement: " << std::endl;
    for (auto op : scratch_stack)
      std::cout << op << " ";
    std::cout << std::endl;
    std::cout << "OPS to overwrite: " << std::endl;
    auto ptr = &op_seq[cur_idx+1];
    for (int i = scratch_stack.size() - 1; i >= 0; i--)
      std::cout << *(ptr++) << " <- " << scratch_stack[i] << std::endl;
    
    // merge_into_idx belongs to right partition
    // This fails if there isn't enough memory allocated
    // It either means we did something wrong, or our memory
    // bound is incorrect.
    // [cur_idx+1, merge_into_idx) belongs to left partition (where scratch_stack goes)
    assert(merge_into_idx - cur_idx - 1 >= (int) scratch_stack.size());
    if (scratch_stack.size() > 0) {
      for (int i = scratch_stack.size() - 1; i >= 0; i--)
        op_seq[++cur_idx] = scratch_stack[i];
    }

    scratch_stack.clear();

    // Calculate the number of Postfixes that are unresolved
    // We need extra space on the left for these
    int unresolved_postfixes = 0;
    std::cout << "split_off_idx = " << split_off_idx << std::endl;
    for (size_t i = 0; i < split_off_idx - 1; i++) {
      std::cout << "Size of " << i << " = " << partition_scratch_spaces[i].size() << std::endl;
      unresolved_postfixes += partition_scratch_spaces[i].size() - 1;
    }

    // assert there will be enough space for these unresolved postfixes
    // in the future
    assert(merge_into_idx - unresolved_postfixes > cur_idx);
    std::cout << "unresolved_postfixes: " << unresolved_postfixes << std::endl;

    //Now op_seq and scratch are properly named. We assign them to left and right.
    left.op_seq  = op_seq;
    left.num_ops = cur_idx + 1 + unresolved_postfixes;

    right.op_seq = op_seq + merge_into_idx;
    right.num_ops = num_ops - merge_into_idx;
    //std::cout /*<< total*/ << "(" << len << ") " << " -> " << left.len << ", " << right.len << std::endl;
  
    // Print out final projection
    std::cout << "Final Result: " << std::endl;
    std::cout << "LEFT:  " << left << std::endl;
    std::cout << "RIGHT: " << right << std::endl << std::endl;
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
  struct request {
    uint32_t addr;
    uint32_t access_number;

    inline bool operator< (request oth) const {
      return addr < oth.addr || (addr == oth.addr && access_number < oth.access_number);
    }

    request() = default;
    request(uint32_t a, uint32_t n) : addr(a), access_number(n) {}
  };
  static_assert(sizeof(request) == sizeof(uint64_t));

  struct ChunkOutput {
    std::vector<request> living_requests;
    std::vector<uint32_t> hits_vector;
  };

  struct ChunkInput {
    ChunkOutput output;            // where to place chunk result
    std::vector<request> requests; // living requests and fresh requests
  };
 private:
  // A vector of all requests
  std::vector<request> requests;

  // Vector of operations used in ProjSequence to store memory operations
  std::vector<Op> operations;

  /* This converts the requests into the previous and next vectors
   * Requests is copied, not modified.
   * Precondition: requests must be properly populated.
   * Returns: number of unique ids in requests
   */
  size_t populate_operations(std::vector<request> &req,
                          std::vector<request> *living_req);

  /* Helper function for update_hits_vector
   * Recursively (and in parallel) populates the distance vector if the
   * projection is small enough, or calls itself with smaller projections otherwise.
   */
  void do_projections(std::vector<uint32_t>& distance_vector, ProjSequence seq);
 
  /*
   * Helper function for solving a projected sequence using the brute force algorithm
   * This takes time O(n^2) but requires no recursion or other overheads. Thus, we can
   * use it to solve larger ProjSequences.
   */
  void do_base_case(std::vector<uint32_t>& distance_vector, ProjSequence seq);

  /*
   * Update a hits vector with the stack depths of the memory requests found in reqs
   * reqs:        vector of memory requests to update the hits vector with
   * hits_vector: A hits vector indicates the number of requests that required a given memory amount
   */
  void update_hits_vector(std::vector<request>& reqs, std::vector<uint32_t>& hits_vector,
                          std::vector<request> *living_req=nullptr);
 public:
  // Logs a memory access to simulate. The order this function is called in matters.
  void memory_access(uint32_t addr);
  /* Returns the success function.
   * Does *a lot* of work.
   * When calling print_success_function, the answer is re-computed.
   */
  SuccessVector get_success_function();

  /*
   * Process a chunk of requests (called by IAF_Wrapper)
   * Return the new living requests and the success function
   */
  void process_chunk(ChunkInput &input);

  IncrementAndFreeze() = default;
  ~IncrementAndFreeze() = default;
};

#endif  // ONLINE_CACHE_SIMULATOR_INCREMENT_AND_FREEZE_H_
