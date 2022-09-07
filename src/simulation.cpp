#include <stdlib.h>

#include <chrono>
#include <cinttypes>
#include <fstream>
#include <random>
#include <set>
#include <utility>

#include "IAKWrapper.h"
#include "IncrementAndFreeze.h"
#include "OSTCacheSim.h"
#include "params.h"

using std::chrono::duration;
using std::chrono::high_resolution_clock;

struct SimResult {
  SuccessVector success;
  double latency;
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

// An enum describing the different CacheSims
enum CacheSimType {
	OS_TREE,
	IAK,
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

CacheSim *new_simulator(CacheSimType sim_enum) {
  CacheSim *sim;

  switch(sim_enum) {
    case OS_TREE:
      sim = new OSTCacheSim();
      break;
    case IAK:
      sim = new IncrementAndFreeze();
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
SimResult run_workloads(CacheSimType sim_enum) {
  CacheSim *sim;
  SimResult first_result;
  SimResult result;


  // Print header
  switch(sim_enum) {
    case OS_TREE:
      std::cout << "Testing OSTree LRU Sim" << std::endl; break;
    case IAK:
      std::cout << "Testing IncrementAndFreeze LRU Sim" << std::endl; break;
    case CHUNK_IAK:
      std::cout << "Testing Chunked IAK LRU Sim" << std::endl; break;
    default:
      std::cerr << "ERROR: Unrecognized sim_enum!" << std::endl;
      exit(EXIT_FAILURE);
  }

  // uniform accesses simulation
  sim = new_simulator(sim_enum);
  first_result = uniform_simulator(sim, SEED);
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
	return first_result;
}

int main(int argc, char **argv) {
  bool verify = false;
  if (argc == 2 && std::string(argv[1]) == "--verify") verify = true;

  SimResult os_res  = run_workloads(OS_TREE);
  SimResult iak_res = run_workloads(IAK);
  SimResult chk_res = run_workloads(CHUNK_IAK);

  if (verify) {
    std::cout << "OSTree and IAK are: ";
    std::cout << (os_res == iak_res ? "equivalent" : "ERROR: different") << std::endl;

    std::cout << "OSTree and CHUNK_IAK are: ";
    std::cout << (os_res == chk_res ? "equivalent" : "ERROR: different") << std::endl;
  }
}
