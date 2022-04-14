#include <stdlib.h>

#include <cinttypes>
#include <fstream>
#include <random>
#include <set>
#include <utility>
#include <chrono>

#include "IAKWrapper.h"
#include "IncrementAndKillMinInPlace.h"
#include "IncrementAndKillSmallInPlace.h"
#include "IncrementAndKillInPlace.h"
#include "IncrementAndKill.h"
#include "LruSizesSim.h"

void print_distance_vector(MinInPlace::IncrementAndKill* iak)
{
  auto good_vector = iak->get_distance_vector();

  for (size_t i = 0; i < good_vector.size(); i++)
  {
      std::cout << "@ " << i << ", " << good_vector[i] << std::endl;
  }
}


void validate_distance_vectors(InPlace::IncrementAndKill* iak, SmallInPlace::IncrementAndKill* iak2)
{
  auto good_vector = iak->get_distance_vector();
  auto test_vector = iak2->get_distance_vector();

  assert(good_vector.size() == test_vector.size());
  bool good = true;
  for (size_t i = 0; i < good_vector.size(); i++)
  {
    if (good_vector[i] != test_vector[i])
    {
      good = false;
      std::cout << "@ " << i << ", Good: " << good_vector[i] << ", " << test_vector[i] <<"." << std::endl;
    }
  }
  if (good)
    std::cout << "DISTANCE VECTORS ARE THE SAME" << std::endl;
}


uint64_t get_next_addr(std::mt19937& gen)
{
  // printf("Representative Workload\n");
  uint64_t working_size = WORKING_SET;
  uint64_t leftover_size = WORKLOAD * working_size;
  double working_chance = gen() / (double)(0xFFFFFFFF);
  uint64_t v_addr = gen();
  if (working_chance <= LOCALITY)
    v_addr %= working_size;
  else
    v_addr = (v_addr % leftover_size) + working_size;
  return v_addr;
}


/*
 * Runs a random sequence of page requests that follow a working_set
 * distribution A majority of accesses are made to a somewhat small working_set
 * while the rest are made randomly to a much larger amount of memory seed: the
 * seed to the random number generator print:    if true print out the results
 * of the simulation returns   the success function
 */
std::vector<std::vector<uint64_t>> working_set_simulator(uint32_t seed, bool print = false) {
  std::set<uint64_t> unique_pages;

  using std::chrono::high_resolution_clock;
  using std::chrono::duration_cast;
  using std::chrono::duration;
  using std::chrono::milliseconds;

  LruSizesSim *lru = new LruSizesSim();
  IncrementAndKill *iak = new IncrementAndKill();
  InPlace::IncrementAndKill *iak2 = new InPlace::IncrementAndKill();
  SmallInPlace::IncrementAndKill *iak3 = new SmallInPlace::IncrementAndKill();
  MinInPlace::IncrementAndKill *iak4 = new MinInPlace::IncrementAndKill();
  IAKWrapper *iak5 = new IAKWrapper();

  //TODO: Make these all CacheSim

  // Order Statistic LRU (stack distance)
  std::mt19937 rand(seed);  // create random number generator
  auto start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    lru->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> lru_success = lru->get_success_function();
  auto lru_time =  duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete lru;

  // Increment And Kill
  rand.seed(seed);  // create random number generator
  start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    iak->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> iak_success = iak->get_success_function();
  auto iak_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete iak;

  // Increment And Kill In Place
  rand.seed(seed);  // create random number generator
  start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    iak2->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> iak2_success = iak2->get_success_function();
  auto iak2_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete iak2;

  // Increment And Kill In Place (half size op array)
  rand.seed(seed);  // create random number generator
  start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    iak3->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> iak3_success = iak3->get_success_function();
  auto iak3_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete iak3;
  
  // Increment And Kill In Place (half size op array + 2 bit type)
  rand.seed(seed);  // create random number generator
  start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    iak4->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> iak4_success = iak4->get_success_function();
  auto iak4_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete iak4;

// Increment And Kill In Place (half size op array + 2 bit type)
  rand.seed(seed);  // create random number generator
  start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    iak5->memory_access(get_next_addr(rand));
  }
  std::vector<uint64_t> iak5_success = iak5->get_success_function();
  auto iak5_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
  delete iak5;

  std::cout << "data to process:" << std::endl;
  rand.seed(seed);  // create random number generator
  for (uint64_t i = 0; i < ACCESSES; i++) {
   std::cout << get_next_addr(rand) << std::endl;
  }

  if (print) {
    // Do this for stats
    start = high_resolution_clock::now();
    rand.seed(seed);  // create random number generator
    for (uint64_t i = 0; i < ACCESSES; i++) {
      unique_pages.insert(get_next_addr(rand));
    }
    auto vec_time = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

    // print the success function if requested
    printf("Out of %" PRIu64 " memory accesses with %lu unique virtual pages\n",
        ACCESSES, unique_pages.size());
    //lru->print_success_function();
    //iak->print_success_function();
    //iak2->print_success_function();
    //iak3->print_success_function();
    validate_distance_vectors(iak2, iak3);
    std::cerr << "VEC TIME: " << vec_time << std::endl;
  }
  std::cerr << "LRU TIME: " << lru_time << std::endl;
  std::cerr << "IAK TIME: " << iak_time << std::endl;
  std::cerr << "IAK IP TIME: " << iak2_time << std::endl;
  std::cerr << "IAK SIP TIME: " << iak3_time << std::endl;
  std::cerr << "IAK MIP TIME: " << iak4_time << std::endl;
  std::cerr << "IAKWrapper TIME: " << iak5_time << std::endl;
  return {lru_success, iak_success, iak2_success, iak3_success, iak4_success, iak5_success};
}

// check that the results of two different simulators are the same
// IMPORTANT: vectors may be of different sizes
bool check_equivalent(std::vector<uint64_t> vec_1, std::vector<uint64_t> vec_2) {
  if (vec_1.size() == 0 || vec_2.size() == 0) return false;
  size_t i = 0;
  uint64_t last_elm = vec_1[0];
  while (i < vec_1.size() && i < vec_2.size()) {
    if (vec_1[i] != vec_2[i]) {std::cout << "DIFF AT " << i << std::endl; return false;}

    last_elm = vec_1[i++];
  }

  // assert that any remaining elements in either vector
  // are identical to last_elm
  for (size_t j = i; j < vec_1.size(); j++)
    if (vec_1[j] != last_elm) return false;

  for (size_t j = i; j < vec_2.size(); j++)
    if (vec_2[j] != last_elm) return false;

  return true;
}

int main() {
  auto results = working_set_simulator(SEED, false);
  auto lru_results = results[0];
  auto iak_results = results[1];
  auto iak2_results = results[2];
  auto iak3_results = results[3];
  auto iak4_results = results[4];
  auto iak_wrapper = results[5];

  for (size_t i = 0; i < iak2_results.size(); i++) {
    if (i < iak_wrapper.size())
      std::cout << iak2_results[i] << " " << iak_wrapper[i] << std::endl;
    else
      std::cout << iak2_results[i] << " EMPTY" << std::endl;
  }

  bool eq = check_equivalent(lru_results, iak_results);
  std::cerr << "Are results equivalent?: " << (eq? "yes" : "no") << std::endl;
  std::cerr << "Sizes (lru, iak): " << lru_results.size() << ", " << iak_results.size() << std::endl;
  eq = check_equivalent(iak_results, iak2_results);
  std::cerr << "Are results equivalent?: " << (eq? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak, iak2): " << iak_results.size() << ", " << iak2_results.size() << std::endl;
  eq = check_equivalent(iak2_results, iak3_results);
  std::cerr << "Are results equivalent?: " << (eq? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak2, iak3): " << iak2_results.size() << ", " << iak3_results.size() << std::endl;
  eq = check_equivalent(iak3_results, iak4_results);
  std::cerr << "Are results equivalent?: " << (eq? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak3, iak4): " << iak3_results.size() << ", " << iak4_results.size() << std::endl;
  eq = check_equivalent(iak4_results, iak_wrapper);
  std::cerr << "Are results equivalent?: " << (eq? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak4, logu): " << iak4_results.size() << ", " << iak_wrapper.size() << std::endl;
}
