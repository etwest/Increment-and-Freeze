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

#include "gtest/gtest.h"
#include "sim_factory.h"

#include <map>
#include <random>
#include <set>

class CacheSimUnitTests : public testing::TestWithParam<CacheSimType> {};
INSTANTIATE_TEST_SUITE_P(CacheSimSuite, CacheSimUnitTests,
                         testing::Values(OS_TREE, /*OS_SET,*/ IAF, BOUND_IAF));

namespace {
using SuccessVector = CacheSim::SuccessVector;
}  // namespace

// Very simple validation of success function
TEST_P(CacheSimUnitTests, SimpleTest) {
  SimulatorArgs sim_args;
  sim_args.min_chunk = 8;
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), sim_args);

  // add a few updates
  sim->memory_access(1);
  sim->memory_access(2);
  sim->memory_access(1);
  sim->memory_access(1);

  // get success function
  SuccessVector svec = sim->get_success_function();
  EXPECT_GE(svec.size(), 3);  // unique ids + 1
  if (svec.size() >= 3) {
    EXPECT_EQ(svec[1], 1);
    EXPECT_EQ(svec[2], 2);
    for (size_t i = 3; i < svec.size(); i++) {
      EXPECT_EQ(svec[i], 2);  // assert rest only get 2 hits
    }
  }
}

// Validate the success function returned by the CacheSim
TEST_P(CacheSimUnitTests, ValidateSuccess) {
  SimulatorArgs sim_args;
  sim_args.min_chunk = 8;
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), sim_args);

  // add a few updates
  size_t repeats = 20;
  for (size_t i = 0; i < repeats; i++) {
    sim->memory_access(1);
    sim->memory_access(2);
    sim->memory_access(3);
    sim->memory_access(4);

    sim->memory_access(1);
    sim->memory_access(2);
    sim->memory_access(3);
    sim->memory_access(4);

    sim->memory_access(5);
    sim->memory_access(4);
    sim->memory_access(6);
    sim->memory_access(5);
  }

  // get success function
  SuccessVector svec = sim->get_success_function();
  EXPECT_GE(svec.size(), 7);  // unique ids + 1
  if (svec.size() >= 7) {
    EXPECT_EQ(svec[1], 0);
    EXPECT_EQ(svec[2], 1 * repeats);
    EXPECT_EQ(svec[3], 2 * repeats);
    EXPECT_EQ(svec[4], 6 * repeats);
    EXPECT_EQ(svec[5], 7 * repeats - 1);
    EXPECT_EQ(svec[6], 12 * repeats - 6);
    for (size_t i = 7; i < svec.size(); i++) {
      EXPECT_EQ(svec[i], 12 * repeats - 6);  // assert rest are same
    }
  }
}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST_P(CacheSimUnitTests, MultipleSuccessCalls) {
  SimulatorArgs sim_args;
  sim_args.min_chunk = 8;
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), sim_args);

  // add a few updates
  size_t loops = 3;
  size_t rep_per_loop = 10;
  for (size_t l = 0; l < loops; l++) {
    for (size_t i = 0; i < rep_per_loop; i++) {
      sim->memory_access(1);
      sim->memory_access(2);
      sim->memory_access(3);
      sim->memory_access(4);

      sim->memory_access(1);
      sim->memory_access(2);
      sim->memory_access(3);
      sim->memory_access(4);

      sim->memory_access(5);
      sim->memory_access(4);
      sim->memory_access(6);
      sim->memory_access(5);
    }

    // get success function
    SuccessVector svec = sim->get_success_function();
    EXPECT_GE(svec.size(), 7);  // unique ids + 1
    if (svec.size() >= 7) {
      EXPECT_EQ(svec[1], 0);
      EXPECT_EQ(svec[2], 1 * (l + 1) * rep_per_loop);
      EXPECT_EQ(svec[3], 2 * (l + 1) * rep_per_loop);
      EXPECT_EQ(svec[4], 6 * (l + 1) * rep_per_loop);
      EXPECT_EQ(svec[5], 7 * (l + 1) * rep_per_loop - 1);
      EXPECT_EQ(svec[6], 12 * (l + 1) * rep_per_loop - 6);
      for (size_t i = 7; i < svec.size(); i++) {
        EXPECT_EQ(svec[i], 12 * (l + 1) * rep_per_loop - 6);  // assert rest are same
      }
    }
  }
}

// Byte-weighted LRU reference. An access to a page seen before hits in every cache of at least
// its current size plus the current size of every distinct page accessed since its last access.
static SuccessVector brute_force_weighted(
    const std::vector<std::pair<req_count_t, req_count_t>>& trace) {
  std::vector<size_t> dist_hits(1, 0);
  std::map<req_count_t, size_t> last_access;
  std::map<req_count_t, req_count_t> cur_size;
  for (size_t t = 0; t < trace.size(); t++) {
    auto [addr, nblocks] = trace[t];
    auto it = last_access.find(addr);
    if (it != last_access.end()) {
      size_t dist = nblocks;
      std::set<req_count_t> seen;
      for (size_t k = it->second + 1; k < t; k++)
        if (trace[k].first != addr && seen.insert(trace[k].first).second)
          dist += cur_size[trace[k].first];
      if (dist_hits.size() <= dist) dist_hits.resize(dist + 1, 0);
      dist_hits[dist]++;
    }
    last_access[addr] = t;
    cur_size[addr] = nblocks;
  }
  SuccessVector succ(dist_hits.size(), 0);
  size_t running = 0;
  for (size_t i = 1; i < dist_hits.size(); i++) succ[i] = running += dist_hits[i];
  return succ;
}

// Pages of several blocks whose size changes between accesses, including back-to-back repeats
// that change size. Long enough to recurse past the IAF base case.
TEST_P(CacheSimUnitTests, VariableSizeMatchesBruteForce) {
  if (GetParam() == OS_TREE) GTEST_SKIP() << "OSTCacheSim does not support nblocks";

  std::mt19937_64 rng(12345);
  for (size_t trial = 0; trial < 5; trial++) {
    SimulatorArgs sim_args;
    sim_args.min_chunk = 64;
    std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), sim_args);

    std::vector<std::pair<req_count_t, req_count_t>> trace;
    std::map<req_count_t, req_count_t> size;
    req_count_t addr = 1;
    for (size_t i = 0; i < 4000; i++) {
      if (rng() % 8 != 0)  // otherwise repeat the last page
        addr = 1 + rng() % 80;
      if (!size.count(addr) || rng() % 3 == 0)
        size[addr] = 1 + rng() % 8;
      trace.push_back({addr, size[addr]});
      sim->memory_access(addr, size[addr]);
    }

    SuccessVector expect = brute_force_weighted(trace);
    SuccessVector svec = sim->get_success_function();
    ASSERT_GT(svec.size(), 1);
    size_t n = std::min(svec.size(), expect.size());
    for (size_t i = 1; i < n; i++)
      ASSERT_EQ(svec[i], expect[i]) << "trial " << trial << ", cache size " << i;
    for (size_t i = n; i < svec.size(); i++)
      ASSERT_EQ(svec[i], expect.back()) << "trial " << trial << ", cache size " << i;
  }
}

// Sampled curve. The reference runs the brute force on the sampled requests alone: a reuse whose
// sampled distance is self + others has estimated distance S * others + self, and lands in bucket
// others + ceil(self / S) of the S-block buckets the curve is kept in.
TEST_P(CacheSimUnitTests, SampledVariableSizeMatchesBruteForce) {
  if (GetParam() == OS_TREE) GTEST_SKIP() << "OSTCacheSim does not support nblocks";

  const size_t sampling_rate = 3;
  const size_t S = size_t(1) << sampling_rate;
  std::mt19937_64 rng(54321);
  for (size_t trial = 0; trial < 3; trial++) {
    SimulatorArgs sim_args;
    sim_args.sampling_rate = sampling_rate;
    sim_args.min_chunk = 64;
    std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), sim_args);

    std::vector<std::pair<req_count_t, req_count_t>> sampled;
    std::map<req_count_t, req_count_t> size;
    req_count_t addr = 1;
    for (size_t i = 0; i < 20000; i++) {
      if (rng() % 8 != 0)
        addr = 1 + rng() % 400;
      if (!size.count(addr) || rng() % 3 == 0)
        size[addr] = 1 + rng() % 24;
      if (sim->should_sample(addr)) sampled.push_back({addr, size[addr]});
      sim->memory_access(addr, size[addr]);
    }

    std::vector<size_t> buckets(1, 0);
    std::map<req_count_t, size_t> last_access;
    std::map<req_count_t, req_count_t> cur_size;
    for (size_t t = 0; t < sampled.size(); t++) {
      auto [a, self] = sampled[t];
      auto it = last_access.find(a);
      if (it != last_access.end()) {
        size_t others = 0;
        std::set<req_count_t> seen;
        for (size_t k = it->second + 1; k < t; k++)
          if (sampled[k].first != a && seen.insert(sampled[k].first).second)
            others += cur_size[sampled[k].first];
        size_t b = others + (self + S - 1) / S;
        if (buckets.size() <= b) buckets.resize(b + 1, 0);
        buckets[b]++;
      }
      last_access[a] = t;
      cur_size[a] = self;
    }

    SuccessVector svec = sim->get_success_function();
    ASSERT_GT(svec.size(), 1);
    size_t running = 0;
    for (size_t b = 1; b < buckets.size(); b++) {
      running += buckets[b];
      for (size_t j = 1 + (b - 1) * S; j < 1 + b * S && j < svec.size(); j++)
        ASSERT_EQ(svec[j], running * S) << "trial " << trial << ", cache size " << j;
    }
    for (size_t j = 1 + (buckets.size() - 1) * S; j < svec.size(); j++)
      ASSERT_EQ(svec[j], running * S) << "trial " << trial << ", cache size " << j;
  }
}
