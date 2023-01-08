#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <fstream>

#include "absl/time/clock.h"
#include "sim_factory.h"
#include "params.h"

struct SimResult {
  CacheSim::SuccessVector success;
  absl::Duration latency;
  bool operator== (const SimResult& other) const
  {
    if (success.size() == 0 || other.success.size() == 0) return false;
    size_t i = 0;
    uint64_t last_elm = success[0];
    while (i < success.size() && i < other.success.size()) {
      if (success[i] != other.success[i]) {
        std::cout << "DIFF AT " << i << std::endl;
        return false;
      }
      last_elm = success[i++];
    }

    // assert that any remaining elements in either vector
    // are identical to last_elm
    for (size_t j = i; j < success.size(); j++)
      if (success[j] != last_elm) return false;

    for (size_t j = i; j < other.success.size(); j++)
      if (other.success[j] != last_elm) return false;

    return true;
  }
};

// Runs a random sequence of page requests that follow a working set
// distribution.  A majority of accesses are made to a somewhat small working
// set while the rest are made randomly to a much larger amount of memory.
//  * seed:    The seed to the random number generator.
//  * print:   If true print out the results of the simulation.
//  * returns: The success function and time it took to compute.
SimResult working_set_simulator(CacheSim &sim, uint32_t seed, bool print = false) {
  std::mt19937_64 rand(seed);  // create random number generator
  auto start = absl::Now();
  for (uint64_t i = 0; i < kAccesses; i++) {
    // compute the next address
    uint64_t working_size = kWorkingSet;
    uint64_t leftover_size = kIdUniverseSize - kWorkingSet;
    double working_chance = rand() / (double)(0xFFFFFFFF);
    uint64_t addr = rand();
    if (working_chance <= kLocality)
      addr %= working_size;
    else
      addr = (addr % leftover_size) + working_size;

    // access the address
    sim.memory_access(addr);
  }
  CacheSim::SuccessVector succ = sim.get_success_function();
  auto duration = absl::Now() - start;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim.print_success_function();
  }
  return {succ, duration};
}

SimResult uniform_simulator(CacheSim &sim, uint32_t seed, bool print = false) {
  std::mt19937_64 rand(seed); // create random number generator
  auto start = absl::Now();
  for (uint64_t i = 0; i < kAccesses; i++) {
    // compute the next address
    uint64_t addr = rand() % kIdUniverseSize;

    // access the address
    sim.memory_access(addr);
  }
  CacheSim::SuccessVector succ = sim.get_success_function();
  auto duration = absl::Now() - start;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim.print_success_function();
  }
  return {succ, duration};
}

SimResult simulate_on_seq(CacheSim &sim, std::vector<uint64_t>& seq, bool print = false) {
  auto start = absl::Now();
  for (uint64_t i = 0; i < seq.size(); i++) {
    // access the address
    sim.memory_access(seq[i]);
  }
  CacheSim::SuccessVector succ = sim.get_success_function();
  auto duration = absl::Now() - start;
  if (print) {
    std::cout << "Success function: " << std::endl;
    sim.print_success_function();
  }
  return {succ, duration};
}

std::vector<uint64_t> generate_zipf(uint32_t seed, double alpha) {
  // std::ofstream zipf_hist("Zipf_hist_" + std::to_string(alpha) + ".data");
  std::mt19937_64 rand(seed); // create random number generator
  std::vector<double> freq_vec;
  freq_vec.reserve(kIdUniverseSize);
  // generate the divisor
  double divisor = 0;
  for (uint64_t i = 1; i < kIdUniverseSize + 1; i++) {
    divisor += 1 / pow(i, alpha);
  }

  // now for each id calculate it's normalized frequency
  for (uint64_t i = 1; i < kIdUniverseSize + 1; i++)
    freq_vec.push_back((1 / pow(i, alpha)) / divisor);

  // now push to sequence vector based upon frequency
  std::vector<uint64_t> seq_vec;
  seq_vec.reserve(kAccesses);
  for (uint64_t i = 0; i < kIdUniverseSize; i++) {
    uint64_t num_items = round(freq_vec[i] * kAccesses);
    // zipf_hist << i << ":" << num_items << std::endl;
    for (uint64_t j = 0; j < num_items && seq_vec.size() < kAccesses; j++)
      seq_vec.push_back(i);
  }

  // if we have too few accesses make up for it by adding more to most common
  if (seq_vec.size() < kAccesses) {
    uint64_t num_needed = kAccesses - seq_vec.size();
    for (uint64_t i = 0; i < num_needed; i++)
      seq_vec.push_back(i % kIdUniverseSize);
  }

  seq_vec.resize(kAccesses);

  // shuffle the sequence vector
  std::shuffle(seq_vec.begin(), seq_vec.end(), rand);
  // std::cout << "Zipfian sequence memory impact: " << seq_vec.size() * sizeof(uint64_t)/1024/1024 << "MiB" << std::endl;
  return seq_vec;
}

constexpr char ArgumentsString[] = "Arguments: sim, workload, [zipf_alpha]\n\
sim:        Which simulator to use. One of: 'OS_TREE', 'OS_SET', 'IAF', 'CHUNK_IAF', 'K_LIM_IAF'\n\
workload:   Which synthetic workload to run. One of: 'uniform', 'zipfian'\n\
zipf_alpha: If running Zipfian workload then provide the alpha value";

// Run a given system and workload combination
// print the latency of the workload and the amount of memory used
// arguments are sim, workload, and zipfian parameter if applicable
int main(int argc, char** argv) {
  if (argc < 3 || argc > 4) {
    std::cerr << "ERROR: Incorrect number of arguments!" << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse sim argument
  std::unique_ptr<CacheSim> sim;
  std::string sim_arg = argv[1];
  if (sim_arg == "OS_TREE")        sim = new_simulator(OS_TREE);
  else if (sim_arg == "OS_SET")    sim = new_simulator(OS_SET);
  else if (sim_arg == "IAF")       sim = new_simulator(IAF);
  else if (sim_arg == "CHUNK_IAF") sim = new_simulator(CHUNK_IAF);
  else if (sim_arg == "K_LIM_IAF") sim = new_simulator(CHUNK_IAF, 65536, kMemoryLimit);
  else {
    std::cerr << "ERROR: Did not recognize simulator: " << sim_arg << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse workload argument and run workload
  std::string workload_arg = argv[2];
  SimResult result;
  size_t memory_usage;
  if (workload_arg == "uniform") {
    std::cout << "Uniform" << std::endl;
    if (argc == 4) std::cerr << "WARNING: Ignoring argument " << argv[3] << std::endl;
    
    std::cout << "Running experiment...         \r"; fflush(stdout);
    result = uniform_simulator(*sim, kSeed);
    memory_usage = sim->get_memory_usage();
  } 
  else if (workload_arg == "zipfian") {
    if (argc != 4) {
      std::cerr << "ERROR: No zipfian alpha value provided." << std::endl;
      std::cerr << ArgumentsString << std::endl;
      exit(EXIT_FAILURE);
    }
    std::cout << "Zipfian: " << std::atof(argv[3]) << std::endl;
    std::cout << "Generating zipfian sequence...\r"; fflush(stdout);
    std::vector<uint64_t> zipf_seq = generate_zipf(kSeed, std::atof(argv[3]));
    size_t zipf_seq_mib = zipf_seq.size() * sizeof(uint64_t) / (1024*1024);
    std::cout << "Running experiment...         \r"; fflush(stdout);
    result = simulate_on_seq(*sim, zipf_seq);

    assert(zipf_seq_mib < sim->get_memory_usage());
    memory_usage = sim->get_memory_usage() - zipf_seq_mib;
  } else {
    std::cerr << "Did not recognize workload: " << workload_arg << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cout << "                              \r";
  std::cout << "Latency      = " << result.latency << std::endl;
  std::cout << "Memory (MiB) = " << memory_usage << std::endl;

  // Output results to temporary csv files for integration with bash script
  std::ofstream latency_csv("tmp_latency.csv", std::ios::app);
  std::ofstream memory_csv("tmp_memory.csv", std::ios::app);
  latency_csv << ", " << absl::ToDoubleSeconds(result.latency);
  memory_csv << ", " << memory_usage;
}
