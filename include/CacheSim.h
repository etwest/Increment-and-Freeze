#pragma once

#include <iostream>
#include <vector>

typedef std::vector<uint64_t> SuccessVector;

class CacheSim {
 protected:
  uint64_t access_number = 1;  // simulated timestamp
 public:
  CacheSim() = default;
  virtual ~CacheSim() = default;
  /*
   * Perform a memory access upon a given id
   * addr:    the id to access 
   * returns  nothing
   */
  virtual void memory_access(uint64_t addr) = 0;

  virtual SuccessVector get_success_function() = 0;

  void print_success_function() {
    // TODO: This is too verbose for real data.
    std::vector<uint64_t> func = get_success_function();
    for (uint64_t page = 1; page < func.size(); page++)
      std::cout << page << ": " << func[page] << std::endl;
  }
};
