#pragma once

#include "CacheSim.h"

enum OpType {Increment, Kill};

class Op{
  //Do we increment or kill?
  OpType type;
  // Increment start index / Kill index
  uint64_t start;
  // Increment End index
  uint64_t end;
  // Increment amount
  uint64_t r;

  Op(uint64_t start, uint64_t end): type(Increment), start(start), end(end), r(1){};
  Op(uint64_t kill): type(Kill), start(kill) {};
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

