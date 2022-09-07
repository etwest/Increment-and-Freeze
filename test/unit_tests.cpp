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
  ASSERT_GE(svec.size(), 3);
  ASSERT_EQ(svec[1], 1);
  ASSERT_EQ(svec[2], 2);
  for (size_t i = 3; i < svec.size(); i++) {
    ASSERT_EQ(svec[i], 2); // assert rest only get 2 hits
  }
}

// Validate the success function returned by the CacheSim
TEST(CacheSimUnitTests, ValidateSuccess) {

}

// Validate the success function returned by the CacheSim
// when multiple calls are made to get_success_function
TEST(CacheSimUnitTests, MultipleSuccessCalls) {

}
