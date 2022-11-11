#include "gtest/gtest.h"
#include "sim_factory.h"

class CacheSimUnitTests : public testing::TestWithParam<CacheSimType> {

};
INSTANTIATE_TEST_SUITE_P(CacheSimSuite, CacheSimUnitTests, testing::Values(OS_TREE, OS_SET, IAK, CHUNK_IAK));

namespace {
using SuccessVector = CacheSim::SuccessVector;
}  // namespace

// Very simple validation of success function
TEST_P(CacheSimUnitTests, SimpleTest) {
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), 8);

  // add a few updates
  sim->memory_access(1);
  sim->memory_access(2);
  sim->memory_access(1);
  sim->memory_access(1);

  // get success function
  SuccessVector svec = sim->get_success_function();
  EXPECT_GE(svec.size(), 3); // unique ids + 1
  if (svec.size() >= 3) {
    EXPECT_EQ(svec[1], 1);
    EXPECT_EQ(svec[2], 2);
    for (size_t i = 3; i < svec.size(); i++) {
      EXPECT_EQ(svec[i], 2); // assert rest only get 2 hits
    }
  }
}

// Validate the success function returned by the CacheSim
TEST_P(CacheSimUnitTests, ValidateSuccess) {
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), 8);

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
  EXPECT_GE(svec.size(), 7); // unique ids + 1
  if (svec.size() >= 7) {
    EXPECT_EQ(svec[1], 0);
    EXPECT_EQ(svec[2], 1 * repeats);
    EXPECT_EQ(svec[3], 2 * repeats);
    EXPECT_EQ(svec[4], 6 * repeats);
    EXPECT_EQ(svec[5], 7 * repeats - 1);
    EXPECT_EQ(svec[6], 12 * repeats - 6);
    for (size_t i = 7; i < svec.size(); i++) {
      EXPECT_EQ(svec[i], 12 * repeats - 6); // assert rest are same
    }
  }
}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST_P(CacheSimUnitTests, MultipleSuccessCalls) {
  std::unique_ptr<CacheSim> sim = new_simulator(GetParam(), 8);

  // add a few updates
  size_t loops        = 3;
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
    EXPECT_GE(svec.size(), 7); // unique ids + 1
    if (svec.size() >= 7) {
      EXPECT_EQ(svec[1], 0);
      EXPECT_EQ(svec[2], 1 * (l+1) * rep_per_loop);
      EXPECT_EQ(svec[3], 2 * (l+1) * rep_per_loop);
      EXPECT_EQ(svec[4], 6 * (l+1) * rep_per_loop);
      EXPECT_EQ(svec[5], 7 * (l+1) * rep_per_loop - 1);
      EXPECT_EQ(svec[6], 12 * (l+1) * rep_per_loop - 6);
      for (size_t i = 7; i < svec.size(); i++) {
        EXPECT_EQ(svec[i], 12 * (l+1) * rep_per_loop - 6); // assert rest are same
      }
    }
  }
}
