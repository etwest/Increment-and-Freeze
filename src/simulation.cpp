#include <stdlib.h>

#include <chrono>
#include <cinttypes>
#include <fstream>
#include <random>
#include <set>
#include <utility>

#include "IAKWrapper.h"
#include "IncrementAndKill.h"
#include "IncrementAndKillInPlace.h"
#include "IncrementAndKillMinInPlace.h"
#include "IncrementAndKillSmallInPlace.h"
#include "LruSizesSim.h"
#include "params.h"

using std::chrono::duration;
using std::chrono::high_resolution_clock;

struct SimResult {
  SuccessVector success;
  double latency;
};

/*
 * Runs a random sequence of page requests that follow a working set distribution.
 * A majority of accesses are made to a somewhat small working set while the rest
 * are made randomly to a much larger amount of memory
 * seed:    The seed to the random number generator.
 * print:   If true print out the results of the simulation.
 * returns: The success function and time it took to compute.
 */
SimResult working_set_simulator(CacheSim *sim, uint32_t seed, bool print = false) {
  std::mt19937 rand(seed);  // create random number generator
  auto start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    // compute the next address
    uint64_t working_size = WORKING_SET;
    uint64_t leftover_size = UNIQUE_IDS - WORKING_SET;
    double working_chance = rand() / (double)(0xFFFFFFFF);
    uint64_t addr = rand();
    if (working_chance <= LOCALITY)
      addr %= working_size;
    else
      addr = (addr % leftover_size) + working_size;

    // access the address
    sim->memory_access(addr);
  }
  SuccessVector succ = sim->get_success_function();
  double time = duration<double>(high_resolution_clock::now() - start).count() * 1e3;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim->print_success_function();
  }
  return {succ, time};
}

SimResult uniform_simulator(CacheSim *sim, uint32_t seed, bool print = false) {
  std::mt19937 rand(seed); // create random number generator
  auto start = high_resolution_clock::now();
  for (uint64_t i = 0; i < ACCESSES; i++) {
    // compute the next address
    uint64_t addr = rand() % UNIQUE_IDS;

    // access the address
    sim->memory_access(addr);
  }
  SuccessVector succ = sim->get_success_function();
  double time = duration<double>(high_resolution_clock::now() - start).count() * 1e3;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim->print_success_function();
  }
  return {succ, time};
}

SimResult simulate_on_seq(CacheSim *sim, std::vector<uint64_t> seq, bool print = false) {
  auto start = high_resolution_clock::now();
  for (uint64_t i = 0; i < seq.size(); i++) {
    // access the address
    sim->memory_access(seq[i]);
  }
  SuccessVector succ = sim->get_success_function();
  double time = duration<double>(high_resolution_clock::now() - start).count() * 1e3;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim->print_success_function();
  }
  return {succ, time};
}

std::vector<uint64_t> generate_zipf(uint32_t seed, double alpha) {
  std::mt19937 rand(seed); // create random number generator
  std::vector<double> freq_vec;
  // generate the divisor
  double divisor = 0;
  for (uint64_t i = 1; i < UNIQUE_IDS + 1; i++) {
    divisor += 1 / pow(i, alpha);
  }

  // now for each id calculate it's normalized frequency
  for (uint64_t i = 1; i < UNIQUE_IDS + 1; i++)
    freq_vec.push_back((1 / pow(i, alpha)) / divisor);

  // now push to sequence vector based upon frequency
  std::vector<uint64_t> seq_vec;
  for (uint64_t i = 0; i < UNIQUE_IDS; i++) {
    uint64_t num_items = round(freq_vec[i] * ACCESSES);
    for (uint64_t j = 0; j < num_items && seq_vec.size() < ACCESSES; j++)
      seq_vec.push_back(i);
  }

  // if we have too few accesses make up for it by adding more to most common
  if (seq_vec.size() < ACCESSES) {
    uint64_t num_needed = ACCESSES - seq_vec.size();
    for (uint64_t i = 0; i < num_needed; i++)
      seq_vec.push_back(i % UNIQUE_IDS);
  }

  // shuffle the sequence vector
  std::shuffle(seq_vec.begin(), seq_vec.end(), rand);
  return seq_vec;
}

// check that the results of two different simulators are the same
// IMPORTANT: vectors may be of different sizes
bool check_equivalent(std::vector<uint64_t> vec_1, std::vector<uint64_t> vec_2) {
  if (vec_1.size() == 0 || vec_2.size() == 0) return false;
  size_t i = 0;
  uint64_t last_elm = vec_1[0];
  while (i < vec_1.size() && i < vec_2.size()) {
    if (vec_1[i] != vec_2[i]) {
      std::cout << "DIFF AT " << i << std::endl;
      return false;
    }

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
  SuccessVector lru_results;
  SuccessVector iak_results;
  SuccessVector iak2_results;
  SuccessVector iak3_results;
  SuccessVector iak4_results;
  SuccessVector iak_wrapper;

#if 0
  { // Order Statistics Tree
    LruSizesSim LRU;
    SimResult result = working_set_simulator(&LRU, SEED);
    std::cout << "LRU: " << result.latency << " ms" << std::endl;
    lru_results = result.success;
  }
  { // Uniform Accesses
    LruSizesSim LRU;
    SimResult result = uniform_simulator(&LRU, SEED);
    std::cout << "Uniform LRU: " << result.latency << " ms" << std::endl;
  }
#endif

#if 0
  { // IncrementAndKill -- working set sim
    IncrementAndKill iak;
    SimResult result = working_set_simulator(&iak, SEED);
    std::cout << "IncrementAndKill: " << result.latency << " ms" << std::endl;
    iak_results = result.success;
  }
  { // Uniform Accessses
    IncrementAndKill iak;
    SimResult result = uniform_simulator(&iak, SEED);
    std::cout << "Uniform IncrementAndKill: " << result.latency << " ms" << std::endl;
  }
#endif

#if 0
  { // InPlace IncrementAndKill -- working set sim
    InPlace::IncrementAndKill iak;
    SimResult result = working_set_simulator(&iak, SEED);
    std::cout << "InPlace IAK: " << result.latency << " ms" << std::endl;
    iak2_results = result.success;
  }
  { // Uniform Accessses
    InPlace::IncrementAndKill iak;
    SimResult result = uniform_simulator(&iak, SEED);
    std::cout << "Uniform InPlace IAK: " << result.latency << " ms" << std::endl;
  }
#endif

#if 0
  { // Smaller InPlace IncrementAndKill -- working set sim
    SmallInPlace::IncrementAndKill iak;
    SimResult result = working_set_simulator(&iak, SEED);
    std::cout << "Smaller Inplace IAK: " << result.latency << " ms" << std::endl;
    iak3_results = result.success;
  }
  { // Uniform Accessses
    SmallInPlace::IncrementAndKill iak;
    SimResult result = uniform_simulator(&iak, SEED);
    std::cout << "Uniform Smaller Inplace IAK: " << result.latency << " ms" << std::endl;
  }
#endif

#if 1
  { // Minimum InPlace IncrementAndKill -- working set sim
    MinInPlace::IncrementAndKill iak;
    SimResult result = working_set_simulator(&iak, SEED);
    std::cout << "Min InPlace IAK: " << result.latency << " ms" << std::endl;
    iak4_results = result.success;
  }
  { // Uniform Accessses
    MinInPlace::IncrementAndKill iak;
    SimResult result = uniform_simulator(&iak, SEED);
    std::cout << "Uniform Min InPlace IAK: " << result.latency << " ms" << std::endl;
  }
#endif

#if 1
  { // Chunked IncrementAndKill -- working set sim
    IAKWrapper iak;
    SimResult result = working_set_simulator(&iak, SEED);
    std::cout << "Chunked IAK: " << result.latency << " ms" << std::endl;
    iak_wrapper = result.success;
  }
  { // Uniform Accessses
    IAKWrapper iak;
    SimResult result = uniform_simulator(&iak, SEED);
    std::cout << "Uniform Chunked IAK: " << result.latency << " ms" << std::endl;
  }
#endif

  std::vector<uint64_t> zipf_seq = generate_zipf(SEED, 1);
  std::cout << "Zipf sequence length = " << zipf_seq.size() << std::endl;
  
  { // Zipfian Accesses
    IAKWrapper iak;
    SimResult result = simulate_on_seq(&iak, zipf_seq);
    std::cout << "Zipf Chunked IAK: " << result.latency << " ms" << std::endl;
  }


  // check correctness using working_set_simulator results
  bool eq = check_equivalent(lru_results, iak_results);
  std::cerr << "Are results equivalent?: " << (eq ? "yes" : "no") << std::endl;
  std::cerr << "Sizes (lru, iak): " << lru_results.size() << ", " << iak_results.size()
            << std::endl;
  eq = check_equivalent(iak_results, iak2_results);
  std::cerr << "Are results equivalent?: " << (eq ? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak, iak2): " << iak_results.size() << ", " << iak2_results.size()
            << std::endl;
  eq = check_equivalent(iak2_results, iak3_results);
  std::cerr << "Are results equivalent?: " << (eq ? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak2, iak3): " << iak2_results.size() << ", " << iak3_results.size()
            << std::endl;
  eq = check_equivalent(iak3_results, iak4_results);
  std::cerr << "Are results equivalent?: " << (eq ? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak3, iak4): " << iak3_results.size() << ", " << iak4_results.size()
            << std::endl;
  eq = check_equivalent(iak4_results, iak_wrapper);
  std::cerr << "Are results equivalent?: " << (eq ? "yes" : "no") << std::endl;
  std::cerr << "Sizes (iak4, logu): " << iak4_results.size() << ", " << iak_wrapper.size()
            << std::endl;
}
