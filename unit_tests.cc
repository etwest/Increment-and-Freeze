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

class CacheSimUnitTests : public testing::TestWithParam<CacheSimType> {};
INSTANTIATE_TEST_SUITE_P(CacheSimSuite, CacheSimUnitTests,
                         testing::Values(OS_TREE, OS_SET, IAF, BOUND_IAF));

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
