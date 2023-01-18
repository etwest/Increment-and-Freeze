#include <gtest/gtest.h>
#include <random>

#include "bounded_iaf.h"

namespace {
using SuccessVector = CacheSim::SuccessVector;
}  // namespace

TEST(MemoryCutoffTests, AbsurdTest) {
  BoundedIAF sim_limit(16, 1); // use default chunk size of 16 and limit memory to 1 page

  // add a few updates
  sim_limit.memory_access(1);
  sim_limit.memory_access(2);
  sim_limit.memory_access(1);
  sim_limit.memory_access(1);

  // get success function
  SuccessVector lim_vec = sim_limit.get_success_function();

  // validate limited success function
  ASSERT_EQ(lim_vec.size(), 2); // memory limit + 1
  ASSERT_EQ(lim_vec[1], 1); // only one entry
}

TEST(MemoryCutoffTests, ValidateSuccess) {
  BoundedIAF sim_limit(32, 4); // use default chunk size of 32 and limit memory to 4 pages

  // add a few updates
  size_t repeats = 20;
  for (size_t i = 0; i < repeats; i++) {
    sim_limit.memory_access(1);
    sim_limit.memory_access(2);
    sim_limit.memory_access(3);
    sim_limit.memory_access(4);

    sim_limit.memory_access(1);
    sim_limit.memory_access(2);
    sim_limit.memory_access(3);
    sim_limit.memory_access(4);

    sim_limit.memory_access(5);
    sim_limit.memory_access(4);
    sim_limit.memory_access(6);
    sim_limit.memory_access(5);
  }

  // get success function
  SuccessVector svec = sim_limit.get_success_function();
  ASSERT_GE(svec.size(), 5); // memory limit + 1
  ASSERT_EQ(svec[1], 0);
  ASSERT_EQ(svec[2], 1 * repeats);
  ASSERT_EQ(svec[3], 2 * repeats);
  ASSERT_EQ(svec[4], 6 * repeats);
}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST(MemoryCutoffTests, MultipleSuccessCalls) {
  BoundedIAF sim_limit(32, 5); // use default chunk size of 32 and limit memory to 5 pages

  // add a few updates
  size_t loops        = 3;
  size_t rep_per_loop = 10;
  for (size_t l = 0; l < loops; l++) {
    for (size_t i = 0; i < rep_per_loop; i++) {
      sim_limit.memory_access(1);
      sim_limit.memory_access(2);
      sim_limit.memory_access(3);
      sim_limit.memory_access(4);

      sim_limit.memory_access(1);
      sim_limit.memory_access(2);
      sim_limit.memory_access(3);
      sim_limit.memory_access(4);

      sim_limit.memory_access(5);
      sim_limit.memory_access(4);
      sim_limit.memory_access(6);
      sim_limit.memory_access(5);
    }

    // get success function
    SuccessVector svec = sim_limit.get_success_function();
    ASSERT_GE(svec.size(), 6); // memory limit + 1
    ASSERT_EQ(svec[1], 0);
    ASSERT_EQ(svec[2], 1 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[3], 2 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[4], 6 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[5], 7 * (l+1) * rep_per_loop - 1);
  }
}

TEST(MemoryCutoffTests, CompareToIAF) {
  // default chunk size 512 with various memory limits
  std::vector<BoundedIAF> sims {{512, 7}, {512, 11}, {512, 16}, {512, 32}, {512, 64}, {512}};

  // random number generator
  std::mt19937_64 gen(42);
  std::uniform_int_distribution<int> distribution(1,1000);
  
  // add random updates from range [1, 1000)
  for (int i = 0; i < 100000; i++) {
    uint64_t num = distribution(gen);
    for (auto& sim : sims)
        sim.memory_access(num);
  }

  // get ground truth
  std::cout << "No memory limit" << std::endl;
  SuccessVector truth = sims[sims.size() - 1].get_success_function();

  // verify that memory limited simulations are prefixes of eachother and un-limited
  for (size_t i = 0; i < sims.size() - 1; i++) {
    // verify that i is a prefix of truth
    std::cout << "Memory Limit = " << sims[i].get_mem_limit() << std::endl;
    SuccessVector svec = sims[i].get_success_function();

    ASSERT_EQ(svec.size(), sims[i].get_mem_limit() + 1);
    for (size_t j = 0; j < svec.size(); j++)
      ASSERT_EQ(svec[j], truth[j]);
  }
}
