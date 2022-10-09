#include <gtest/gtest.h>

// Include our code
#include "IAKWrapper.h"
#include "IncrementAndFreeze.h"
#include "OSTCacheSim.h"

// An enum describing the different CacheSims
enum CacheSimType {
  OS_TREE,
  IAK,
  CHUNK_IAK,
};

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

class CacheSimUnitTests : public testing::TestWithParam<CacheSimType> {

};
INSTANTIATE_TEST_SUITE_P(CacheSimSuite, CacheSimUnitTests, testing::Values(OS_TREE, IAK, CHUNK_IAK));

namespace {
using SuccessVector = CacheSim::SuccessVector;
}  // namespace

// Very simple validation of success function
TEST_P(CacheSimUnitTests, SimpleTest) {
  CacheSim *sim = new_simulator(GetParam());

  // add a few updates
  sim->memory_access(1);
  sim->memory_access(2);
  sim->memory_access(1);
  sim->memory_access(1);

  // get success function
  SuccessVector svec = sim->get_success_function();
  ASSERT_GE(svec.size(), 3); // unique ids + 1
  ASSERT_EQ(svec[1], 1);
  ASSERT_EQ(svec[2], 2);
  for (size_t i = 3; i < svec.size(); i++) {
    ASSERT_EQ(svec[i], 2); // assert rest only get 2 hits
  }
	delete sim;
}

// Validate the success function returned by the CacheSim
TEST_P(CacheSimUnitTests, ValidateSuccess) {
  CacheSim *sim = new_simulator(GetParam());

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
  ASSERT_GE(svec.size(), 7); // unique ids + 1
  ASSERT_EQ(svec[1], 0);
  ASSERT_EQ(svec[2], 1 * repeats);
  ASSERT_EQ(svec[3], 2 * repeats);
  ASSERT_EQ(svec[4], 6 * repeats);
  ASSERT_EQ(svec[5], 7 * repeats - 1);
  ASSERT_EQ(svec[6], 12 * repeats - 6);
  for (size_t i = 7; i < svec.size(); i++) {
    ASSERT_EQ(svec[i], 12 * repeats - 6); // assert rest are same
  }
	delete sim;
}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST_P(CacheSimUnitTests, MultipleSuccessCalls) {
  CacheSim *sim = new_simulator(GetParam());

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
    ASSERT_GE(svec.size(), 7); // unique ids + 1
    ASSERT_EQ(svec[1], 0);
    ASSERT_EQ(svec[2], 1 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[3], 2 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[4], 6 * (l+1) * rep_per_loop);
    ASSERT_EQ(svec[5], 7 * (l+1) * rep_per_loop - 1);
    ASSERT_EQ(svec[6], 12 * (l+1) * rep_per_loop - 6);
    for (size_t i = 7; i < svec.size(); i++) {
      ASSERT_EQ(svec[i], 12 * (l+1) * rep_per_loop - 6); // assert rest are same
    }
  }
	delete sim;
}
