#include "IncrementAndKill.h"
#include <algorithm>

void IncrementAndKill::memory_access(uint64_t addr)
{
  requests.push_back({addr, access_number++});
}

std::vector<uint64_t> IncrementAndKill::get_success_function()
{
  calculate_prevnext();
  return std::vector<uint64_t>();
}

void IncrementAndKill::calculate_prevnext()
{
  // Here's where the heavy lifting occurs
  
  // put all requests of the same addr next to each other
  // then order those by access_number
  std::sort(requests.begin(), requests.end());
  

  prevnext.resize(requests.size()+1);

  // i-1th element
  tuple last;
  for (int i = 0; i < requests.size(); i++)
  {
    auto [addr, access_num] = requests[i];
    auto [last_addr, last_access_num] = last;
    // Ensure there is a previous access of this
    if (last_access_num > 0 && addr == last_addr)
    {
      // Point this access to the previous one
      prev(access_num) = last_access_num;
      // and the previous access to this one
      next(last_access_num) = access_num;
    }
    else
    {
      // last is different
      prev(access_num) = 0;
    }
    // Preemptively point this one's next access to the end 
    next(access_num) = requests.size()+1;

    last = requests[i];
  }

  for (int i = 0; i < requests.size(); i++)
  {
    auto [addr, access_num] = requests[i];
    std::cout << addr << ": " << access_num << std::endl;
  }
  
  for (int i = 1; i < requests.size()+1; i++)
  {
    auto [addr, access_num] = requests[i];
    std::cout << "prev: " << prev(i) << ", next: " << next(i) << std::endl;
  }
}
