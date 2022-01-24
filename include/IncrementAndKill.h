#pragma once

#include "CacheSim.h"

#include <cassert>

enum OpType {Null, Increment, Kill};

class Op {
 private:
  OpType type;     // Do we increment or kill?
  uint64_t start;  // Increment start index / Kill index
  uint64_t end;    // Increment End index
  uint64_t r;      // Increment amount

 public:
  // create an increment
  Op(uint64_t start, uint64_t end)
      : type(Increment), start(start), end(end), r(1){};

  // create a kill
  Op(uint64_t kill) : type(Kill), start(kill){};

  // create a new Op by projecting another one
  Op(Op oth_op, uint64_t proj_start, uint64_t proj_end);

  Op& operator+=(Op& oth) {
    assert(oth.type == Increment);
    assert(type == Increment);
    assert(start == oth.start);
    assert(end == oth.end);

    r += oth.r;
    return *this;
  }

  // is this operation passive in the projection defined by
  // proj_start and proj_end
  bool is_passive(uint64_t proj_start, uint64_t proj_end) {
    return type == Increment && start <= proj_start && end >= proj_end;
  }
  
  void merge(Op oth_op) {
    assert(oth_op.type == Increment);
    assert(type == Increment);
    assert(start == oth_op.start);
    assert(end == oth_op.end);

    r += oth_op.r;
  }

  OpType get_type() {return type;}
  uint64_t get_r()  {return r;}
};

class ProjSequence {
public:
  std::vector<Op> op_seq;
  uint64_t start;
  uint64_t end;

  ProjSequence(uint64_t start, uint64_t end) : start(start), end(end) {};

  void add_op(Op new_op) {
    Op proj_op = Op(new_op, start, end);
    if (proj_op.get_type() == Null) return;
    if (op_seq.size() == 0) {
      op_seq.push_back(proj_op);
      return;
    }

    Op last_op = op_seq[op_seq.size() - 1];
    if (!proj_op.is_passive(start, end) || !last_op.is_passive(start, end)) {
      op_seq.push_back(proj_op);
      return;
    }
    // merge with the last op in sequence
    last_op.merge(proj_op);
  }
};

class IncrementAndKill: public CacheSim {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    std::vector<tuple> requests;
    std::vector<tuple> prevnext;
    void calculate_prevnext();
    uint64_t& prev(uint64_t i) {return prevnext[i].first;}
    uint64_t& next(uint64_t i) {return prevnext[i].second;}
  public:
    void memory_access(uint64_t addr);
    std::vector<uint64_t> get_success_function();
    IncrementAndKill() = default;
    ~IncrementAndKill() = default;
};
