#include <gtest/gtest.h>

// Include our code
#include "IAKWrapper.h"
#include "IncrementAndFreeze.h"
#include "OSTCacheSim.h"

// Very simple validation of success function
TEST(CacheSimUnitTests, SimpleTest) {
  CacheSim *sim = new OSTCacheSim();

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
}

// Validate the success function returned by the CacheSim
TEST(CacheSimUnitTests, ValidateSuccess) {
  CacheSim *sim = new OSTCacheSim();

  // add a few updates
  size_t repeats = 1;
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
    ASSERT_EQ(svec[i], 12 * repeats - 6); // assert rest only get 2 hits
  }
}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST(CacheSimUnitTests, MultipleSuccessCalls) {
  CacheSim *sim = new OSTCacheSim();

  // add a few updates
  sim->memory_access(1);
  sim->memory_access(2);
  sim->memory_access(1);
  sim->memory_access(1);

  // get success function
  SuccessVector svec = sim->get_success_function();
  ASSERT_GE(svec.size(), 3);
  ASSERT_EQ(svec[1], 1);
  ASSERT_EQ(svec[2], 2);
  for (size_t i = 3; i < svec.size(); i++) {
    ASSERT_EQ(svec[i], 2); // assert rest only get 2 hits
  }
}
