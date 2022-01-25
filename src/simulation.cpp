#include "../include/RAM.h"
#include "IncrementAndKill.h"
#include <cinttypes>
#include <set>
#include <fstream>
#include <stdlib.h>
#include <utility>
#include <random>

/*
 * Runs a random sequence of page requests that follow a working_set distribution
 * A majority of accesses are made to a somewhat small working_set while the rest
 * are made randomly to a much larger amount of memory
 * seed:     the seed to the random number generator
 * print:    if true print out the results of the simulation
 * returns   the success function
 */
std::vector<uint64_t> working_set_simulator(uint32_t seed, bool print=false) {
    std::set<uint64_t> unique_pages;

    LruSizesSim *lru = new LruSizesSim();

    // printf("Representative Workload\n");
    uint64_t working_size  = WORKING_SET / PAGE_SIZE;
    uint64_t leftover_size = WORKLOAD * working_size;
    std::mt19937 rand(seed); // create random number generator

    for(uint64_t i = 0; i < ACCESSES; i++) {
        double working_chance = rand() / (double) (0xFFFFFFFF);
        uint64_t v_addr = rand();
        if(working_chance <= LOCALITY)
            v_addr %= working_size;
        else
            v_addr = (v_addr % leftover_size) + working_size;

        // printf("Memory access %i\n", i);
        lru->memory_access(v_addr);

        unique_pages.insert(v_addr);
    }
    if (print) {
        // print the success function if requested
        printf("Out of %" PRIu64 " memory accesses with %lu unique virtual pages\n", ACCESSES, unique_pages.size());
        lru->print_success_function();
    }
    std::vector<uint64_t> toret = lru->get_success_function();
    delete lru;
    return toret;
}

/*
 * Runs a predefined sequence of memory accesses given by a Zipfian distribution
 * this sequence of pages is found in ZIPH_FILE
 * print:    if true print out the results of the simulation
 * returns   the success function
 */
std::vector<uint64_t> zipfian_simulator(bool print=false) {
    // open the zipf file
    std::ifstream zipf_data(ZIPH_FILE);
    if (!zipf_data.is_open()) {
        printf("Failed to open zipf file\n");
        exit(EXIT_FAILURE);
    }

    std::set<uint64_t> unique_pages;

    // Create the LRU simulator
    LruSizesSim *lru = new LruSizesSim();
    uint64_t accesses = 0;

    // Run the workload
    printf("Zipfian Workload\n");
    std::string line;
    while(getline(zipf_data, line)) {
        uint64_t v_addr = std::stol(line);
        lru->memory_access(v_addr);
        accesses++;

        unique_pages.insert(v_addr);
    }

    if (print) {
        // print the success function if requested
        printf("Out of %" PRIu64 " memory accesses with %lu unique virtual pages\n", ACCESSES, unique_pages.size());
        lru->print_success_function();
    }

    // close the file and return
    zipf_data.close();
    std::vector<uint64_t> toret = lru->get_success_function();
    delete lru;
    return toret;
}


int main() {

  IncrementAndKill algo;
  for (int i = 0; i < 10; i++)
  {
    algo.memory_access(5 - (i/2));
  }

  algo.print_success_function();
  return 1;
    // run many trials of the working_set_simulator
    std::vector<uint64_t> lru_total;
    uint32_t trials = 10;

    std::mt19937 rand(SEED); // use params seed to make rand() gen for simulator seeds

    // run each trial
    for (uint64_t i = 0; i < trials; i++) {
        // run the simulator
        std::vector<uint64_t> result = working_set_simulator(rand());
        lru_total.resize(result.size());

        // loop through success function and add to lru_total
        for (uint32_t page = 1; page < result.size(); page++)
            lru_total[page] += result[page];

        printf("Trial: %li\r", i); std::fflush(stdout);
    }

    // output the page fault information to a file
    std::ofstream out(OUT_FILE);
    out << "# Final Statistics"  << std::endl;
    out << "# Memory Size = "    << MEM_SIZE << std::endl;
    out << "# Total Accesses = " << ACCESSES << std::endl;
    for (uint32_t page = 1; page < lru_total.size(); page++) {
        lru_total[page] /= trials;
        out << page << ":" << lru_total[page] << std::endl;
    }

    // zipfian_simulator(true);
}
