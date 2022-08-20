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

// An enum describing the different CacheSims
enum CacheSimType {
  OS_TREE,
  IAK,
  INPLACE_IAK,
  SMALL_IAK,
  MIN_IAK,
  CHUNK_IAK,
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
  std::cout << "Zipfian sequence memory impact: " << seq_vec.size() * sizeof(uint64_t)/1024/1024 << "MiB" << std::endl;
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

CacheSim *new_simulator(CacheSimType sim_enum) {
  CacheSim *sim;

  switch(sim_enum) {
    case OS_TREE:
      sim = new LruSizesSim();
      break;
    case IAK:
      sim = new IncrementAndKill();
      break;
    case INPLACE_IAK:
      sim = new InPlace::IncrementAndKill();
      break;
    case SMALL_IAK:
      sim = new SmallInPlace::IncrementAndKill();
      break;
    case MIN_IAK:
      sim = new MinInPlace::IncrementAndKill();
      break;
    case CHUNK_IAK:
      sim = new IAKWrapper();
      break;
    default:
      std::cerr << "ERROR: Unrecognized sim_enum!" << std::endl;
      exit(EXIT_FAILURE);
  }

  return sim;
}

// Run all workloads and record results
void run_workloads(CacheSimType sim_enum) {
  CacheSim *sim;
  SimResult result;

  // Print header
  switch(sim_enum) {
    case OS_TREE:
      std::cout << "Testing OSTree LRU Sim" << std::endl; break;
    case IAK:
      std::cout << "Testing IncrementAndKill LRU Sim" << std::endl; break;
    case INPLACE_IAK:
      std::cout << "Testing Inplace IAK LRU Sim" << std::endl; break;
    case SMALL_IAK:
      std::cout << "Testing Small Inplace IAK LRU Sim" << std::endl; break;
    case MIN_IAK:
      std::cout << "Testing Minimum Inplace IAK LRU Sim" << std::endl; break;
    case CHUNK_IAK:
      std::cout << "Testing Chunked IAK LRU Sim" << std::endl; break;
    default:
      std::cerr << "ERROR: Unrecognized sim_enum!" << std::endl;
      exit(EXIT_FAILURE);
  }

  // uniform accesses simulation
  sim = new_simulator(sim_enum);
  result = uniform_simulator(sim, SEED);
  std::cout << "\tUniform Set Latency = " << result.latency << " ms" << std::endl;
  delete sim;

  // working set simulation
  sim = new_simulator(sim_enum);
  result = working_set_simulator(sim, SEED);
  std::cout << "\tWorking Set Latency = " << result.latency << " ms" << std::endl;
  delete sim;

  // test with different Zipfian parameters
  std::vector<double> zipf_exps{0.1, 0.25, 0.5, 0.75, 1, 1.5, 2.0, 2.5, 3.0};
  for (double exp : zipf_exps) {
    sim = new_simulator(sim_enum);
    std::vector<uint64_t> zipf_seq = generate_zipf(SEED, exp);
    result = simulate_on_seq(sim, zipf_seq);
    std::cout << "\tZipfian, alpha=" << exp << " Latency = " << result.latency << " ms" << std::endl;
    delete sim;
  }
}

int main() {
  // run_workloads(OS_TREE);
  // run_workloads(IAK);
  run_workloads(INPLACE_IAK);
  run_workloads(SMALL_IAK);
  run_workloads(MIN_IAK);
  run_workloads(CHUNK_IAK);
}
