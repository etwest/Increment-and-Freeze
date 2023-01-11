#ifndef ONLINE_CACHE_SIMULATOR_OP_H_
#define ONLINE_CACHE_SIMULATOR_OP_H_

#include <cassert>     // for assert
#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t, uint32_t, int64_t, int32_t
#include <iostream>    // for operator<<, basic_ostream::operator<<, basic_o...
#include <utility>     // for pair, move, swap
#include <vector>      // for vector, vector<>::iterator

//#include "cache_sim.h"  // for CacheSim
#include "params.h"     // for kIafBranching

// Operation types and be Prefix, Postfix, or Null
// Prefix and Postfix are encoded by a single bit at the beginning of _target
// Null is encoded by an entirely zero _target variable
enum OpType {Prefix=0, Postfix=1, Null=2};

// An IAF operation
// Due to design of Op, max number of requests to process at once is 2^63
class Op {
 private:
  uint64_t _target = 0;                   // Boundary of operation
  static constexpr uint64_t inc_amnt = 1; // subrange Increment amount
  int64_t full_amnt = 0;                  // fullrange Increment amount

  static constexpr uint64_t tmask = ~(1l << 63);
  static constexpr uint64_t ntmask = ~tmask;
  void set_target(const uint64_t& new_target) {
    assert(new_target == (new_target & tmask));
    _target &= ntmask;
    _target |= new_target;
  };
  inline void set_type(const OpType& t) {
    _target &= tmask;
    _target |= ((int64_t)t << 63);
  };
 public:
  // create an Prefix (if target is 0 -> becomes a Null op)
  Op(uint64_t target, int64_t full_amnt)
      : full_amnt(full_amnt){set_type(Prefix); set_target(target);};

  // create a Postfix
  Op(uint64_t target){set_type(Postfix); set_target(target);};

  // Uninitialized. Used to parallelize making a vector of this without push_back
  Op() {};

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
    else return (OpType)(_target >> 63);
  }
  inline bool is_null() const          { return _target == 0; }
  inline uint64_t get_target() const   { return _target & tmask; }
  inline uint64_t get_inc_amnt() const { return inc_amnt; }
  inline int64_t get_full_amnt() const { return full_amnt; }
};
#endif  // ONLINE_CACHE_SIMULATOR_OP_H_
