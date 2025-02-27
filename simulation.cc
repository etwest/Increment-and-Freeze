/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel DeLayo, Bradley Kuszmaul, Evan West
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
SimResult working_set_simulator(CacheSim &sim, size_t accesses, size_t universe, uint64_t seed) {
  std::mt19937_64 rand(seed);  // create random number generator
  auto start = absl::Now();
  for (uint64_t i = 0; i < accesses; i++) {
    // compute the next address
    uint64_t working_size = kWorkingSet;
    uint64_t leftover_size = universe - kWorkingSet;
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
  return {succ, duration};
}

SimResult uniform_simulator(CacheSim &sim, size_t accesses, size_t universe, uint64_t seed) {
  std::mt19937_64 rand(seed); // create random number generator
  auto start = absl::Now();
  std::cout << "Performing Accesses...  0%       \r"; fflush(stdout);
  size_t half_percent = accesses / 200;
  size_t last_print = 0;
  size_t cur_half = 0;
  for (uint64_t i = 0; i < accesses; i++) {
    if (i - last_print > half_percent) {
      cur_half += 1;
      std::cout << "Performing Accesses...  " << (float)cur_half/2 << "%        \r"; fflush(stdout);
      last_print = i;
    }
    // compute the next address
    uint64_t addr = rand() % universe;

    // access the address
    sim.memory_access(addr);
  }
  std::cout << "Getting Success Function...       \r"; fflush(stdout);
  CacheSim::SuccessVector succ = sim.get_success_function();
  auto duration = absl::Now() - start;
  return {succ, duration};
}

SimResult simulate_on_seq(CacheSim &sim, std::vector<uint64_t>& seq) {
  auto start = absl::Now();
  std::cout << "Performing Accesses...  0%       \r"; fflush(stdout);
  size_t half_percent = seq.size() / 200;
  size_t last_print = 0;
  size_t cur_half = 0;
  for (uint64_t i = 0; i < seq.size(); i++) {
    if (i - last_print > half_percent) {
      cur_half += 1;
      std::cout << "Performing Accesses...  " << (float)cur_half/2 << "%        \r"; fflush(stdout);
      last_print = i;
    }
    // access the address
    sim.memory_access(seq[i]);
  }
  std::cout << "Getting Success Function...       \r"; fflush(stdout);
  CacheSim::SuccessVector succ = sim.get_success_function();
  auto duration = absl::Now() - start;
  return {succ, duration};
}

std::vector<uint64_t> generate_zipf(uint64_t seed, size_t accesses, size_t universe, double alpha) {
  // std::ofstream zipf_hist("Zipf_hist_" + std::to_string(alpha) + ".data");
  std::mt19937_64 rand(seed); // create random number generator
  std::vector<double> freq_vec;
  freq_vec.reserve(universe);
  // generate the divisor
  double divisor = 0;
  for (uint64_t i = 1; i < universe + 1; i++) {
    divisor += 1 / pow(i, alpha);
  }

  // now for each id calculate it's normalized frequency
  for (uint64_t i = 1; i < universe + 1; i++)
    freq_vec.push_back((1 / pow(i, alpha)) / divisor);

  // now push to sequence vector based upon frequency
  std::vector<uint64_t> seq_vec;
  seq_vec.reserve(accesses);
  for (uint64_t i = 0; i < universe; i++) {
    uint64_t num_items = round(freq_vec[i] * accesses);
    // zipf_hist << i << ":" << num_items << std::endl;
    for (uint64_t j = 0; j < num_items && seq_vec.size() < accesses; j++)
      seq_vec.push_back(i);
  }

  // if we have too few accesses make up for it by adding more to most common
  if (seq_vec.size() < accesses) {
    uint64_t num_needed = accesses - seq_vec.size();
    for (uint64_t i = 0; i < num_needed; i++)
      seq_vec.push_back(i % universe);
  }

  seq_vec.resize(accesses);

  // shuffle the sequence vector
  std::shuffle(seq_vec.begin(), seq_vec.end(), rand);
  // std::cout << "Zipfian sequence memory impact: " << seq_vec.size() * sizeof(uint64_t)/1024/1024 << "MiB" << std::endl;
  return seq_vec;
}

constexpr char ArgumentsString[] = "\
Arguments:  accesses, universe, memory_lim, sampling_rate, out_file, sim, workload, [zipf_alpha]\n\
accesses:      Number of memory accesses to perform\n\
universe:      Number of addresses in the universe\n\
memory_lim:    For 'K_LIM_IAF' limit memory to this many requests\n\
sampling_rate: Sample every 1 in 2^sampling_rate addresses (pass 0 to perform no sampling)\n\
out_file:      The file in which to place the success function\n\
sim:           Which simulator to use: 'OS_TREE', 'OS_SET', 'IAF', 'BOUND_IAF', or 'K_LIM_IAF'\n\
workload:      Which synthetic workload to run: 'uniform' or 'zipfian'\n\
zipf_alpha:    If running Zipfian workload then provide the alpha value";

// Run a given system and workload combination
// print the latency of the workload and the amount of memory used
// arguments are sim, workload, and zipfian parameter if applicable
int main(int argc, char** argv) {
  if (argc < 8 || argc > 9) {
    std::cerr << "ERROR: Incorrect number of arguments!" << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse workload arguments
  size_t accesses = std::stoul(argv[1]);
  size_t universe = std::stoul(argv[2]);

  // parse simulator arguments
  SimulatorArgs simulator_args;
  simulator_args.k_mem_limit = std::stoul(argv[3]);
  simulator_args.sampling_rate = std::stoul(argv[4]);
  simulator_args.min_chunk = 65536;


  // parse output file argument
  std::ofstream succ_file(argv[5]);
  if (!succ_file.is_open()) {
    std::cerr << "ERROR: Could not open out file: " << argv[1] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse sim argument
  std::unique_ptr<CacheSim> sim;
  std::string sim_arg = argv[6];

  if (sim_arg == "OS_TREE")        sim = new_simulator(OS_TREE, simulator_args);
  else if (sim_arg == "OS_SET")    sim = new_simulator(OS_SET, simulator_args);
  else if (sim_arg == "IAF")       sim = new_simulator(IAF, simulator_args);
  else if (sim_arg == "BOUND_IAF") sim = new_simulator(BOUND_IAF, simulator_args);
  else if (sim_arg == "K_LIM_IAF") sim = new_simulator(BOUND_IAF, simulator_args);
  else {
    std::cerr << "ERROR: Did not recognize simulator: " << sim_arg << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // parse workload argument and run workload
  std::string workload_arg = argv[7];
  SimResult result;
  double memory_usage;
  if (workload_arg == "uniform") {
    std::cout << "Uniform" << std::endl;
    if (argc == 9) std::cerr << "WARNING: Ignoring argument " << argv[8] << std::endl;

    result = uniform_simulator(*sim, accesses, universe, kSeed);
    memory_usage = sim->get_memory_usage();
  } 
  else if (workload_arg == "zipfian") {
    if (argc != 9) {
      std::cerr << "ERROR: No zipfian alpha value provided." << std::endl;
      std::cerr << ArgumentsString << std::endl;
      exit(EXIT_FAILURE);
    }
    std::cout << "Zipfian: " << std::atof(argv[8]) << std::endl;
    std::cout << "Generating zipfian sequence... \r"; fflush(stdout);
    std::vector<uint64_t> zipf_seq = generate_zipf(kSeed, accesses, universe, std::atof(argv[8]));
    size_t zipf_seq_mib = zipf_seq.size() * sizeof(uint64_t) / (1024*1024);
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


  // Output the success function to a file
  std::cout << "Writing success function...           \r"; fflush(stdout);
  sim->dump_success_function(succ_file, result.success);
  std::cout << "                                      \r"; fflush(stdout);
}
