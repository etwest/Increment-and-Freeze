
# Compile unit tests
cc_test(
  name = "unit_tests",
  size = "small",
  srcs = ["test/unit_tests.cpp", 
          "include/IncrementAndFreeze.h", 
          "include/OSTCacheSim.h",
          "include/OSTree.h",
          "include/IAKWrapper.h",
          "include/CacheSim.h",
          "include/params.h",
          "src/IncrementAndFreeze.cpp", 
          "src/OSTCacheSim.cpp",
          "src/OSTree.cpp",
          "src/IAKWrapper.cpp",],
  includes = ["include/"],
  deps = ["@googletest//:gtest_main"],
)
