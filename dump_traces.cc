/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel Delayo, Bradley Kuszmaul, Evan West
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
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <fstream>

#include "params.h"
#include "cache_sim.h"

void uniform_trace(std::ofstream& out, uint64_t seed) {
  std::mt19937_64 rand(seed);
  std::cout << "Dumping Trace...  0%       \r"; fflush(stdout);
  size_t half_percent = kAccesses / 200;
  size_t last_print = 0;
  size_t cur_half = 0;
  for (uint64_t i = 0; i < kAccesses; i++) {
    if (i - last_print > half_percent) {
      cur_half += 1;
      std::cout << "Dumping Trace...  " << (float)cur_half/2 << "%        \r"; fflush(stdout);
      last_print = i;
    }
    out << (req_count_t)rand() % kIdUniverseSize << std::endl;
  } 
}

void zipfian_trace(std::ofstream& out, uint64_t seed, double alpha) {
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
  std::vector<req_count_t> seq_vec;
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

  std::cout << "Dumping Trace...  0%       \r"; fflush(stdout);
  size_t half_percent = kAccesses / 200;
  size_t last_print = 0;
  size_t cur_half = 0;
  for (uint64_t i = 0; i < seq_vec.size(); i++) {
    if (i - last_print > half_percent) {
      cur_half += 1;
      std::cout << "Dumping Trace...  " << (float)cur_half/2 << "%        \r"; fflush(stdout);
      last_print = i;
    }
    out << seq_vec[i] << std::endl;
  } 
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "ERROR: Incorrect number of arguments. Need 1" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string dir = argv[1];
  
	std::cout << "Accesses = " << kAccesses << std::endl;
	std::cout << "Universe = " << kIdUniverseSize << std::endl;

  // dump traces to files
  {
    std::cout << "Uniform access trace" << std::endl;
    std::ofstream out(dir + "uniform.trace");
    uniform_trace(out, kSeed);
  }
  {
    std::cout << "Zipfian access trace 0.1" << std::endl;
    std::ofstream out(dir + "zipfian_0.1.trace");
    zipfian_trace(out, kSeed, 0.1);
  }
  {
    std::cout << "Zipfian access trace 0.2" << std::endl;
    std::ofstream out(dir + "zipfian_0.2.trace");
    zipfian_trace(out, kSeed, 0.2);
  }
  {
    std::cout << "Zipfian access trace 0.4" << std::endl;
    std::ofstream out(dir + "zipfian_0.4.trace");
    zipfian_trace(out, kSeed, 0.4);
  }
  {
    std::cout << "Zipfian access trace 0.6" << std::endl;
    std::ofstream out(dir + "zipfian_0.6.trace");
    zipfian_trace(out, kSeed, 0.6);
  }
  {
    std::cout << "Zipfian access trace 0.8" << std::endl;
    std::ofstream out(dir + "zipfian_0.8.trace");
    zipfian_trace(out, kSeed, 0.8);
  }
}
