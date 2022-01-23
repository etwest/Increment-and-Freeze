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
};

struct proj_sequence {
  std::vector<Op> op_seq;
  uint64_t start;
  uint64_t end;
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
