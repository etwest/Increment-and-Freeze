#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <fstream>

#include "params.h"

void uniform_trace(std::ofstream& out, uint32_t seed) {
  std::vector<uint64_t> ret;
  ret.reserve(kAccesses);
  std::mt19937_64 rand(seed);
  for (uint64_t i = 0; i < kAccesses; i++)
    out << rand() % kIdUniverseSize << std::endl;
}

void zipfian_trace(std::ofstream& out, uint32_t seed, double alpha) {
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
  for (auto addr : seq_vec)
    out << addr << std::endl;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "ERROR: Incorrect number of arguments. Need 1" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string dir = argv[1];

  // dump traces to files
  {
    std::cout << "Unique access trace . . .                \r"; fflush(stdout);
    std::ofstream out(dir + "unique_accesses.trace");
    uniform_trace(out, kSeed);
  }
  {
    std::cout << "Zipfian access trace 0.1 . . .           \r"; fflush(stdout);
    std::ofstream out(dir + "zipfian_0.1.trace");
    zipfian_trace(out, kSeed, 0.1);
  }
  {
    std::cout << "Zipfian access trace 0.2 . . .           \r"; fflush(stdout);
    std::ofstream out(dir + "zipfian_0.2.trace");
    zipfian_trace(out, kSeed, 0.2);
  }
  {
    std::cout << "Zipfian access trace 0.4 . . .           \r"; fflush(stdout);
    std::ofstream out(dir + "zipfian_0.4.trace");
    zipfian_trace(out, kSeed, 0.4);
  }
  {
    std::cout << "Zipfian access trace 0.6 . . .           \r"; fflush(stdout);
    std::ofstream out(dir + "zipfian_0.6.trace");
    zipfian_trace(out, kSeed, 0.6);
  }
  {
    std::cout << "Zipfian access trace 0.8 . . .           \r"; fflush(stdout);
    std::ofstream out(dir + "zipfian_0.8.trace");
    zipfian_trace(out, kSeed, 0.8);
  }
}