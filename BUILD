test_suite(
    name = "tests",
    tests = ["unit_tests"],
)

cc_binary(
	name = "sim",
	deps = [
		"CacheSim"
	],
	srcs = [
		"src/simulation.cpp",
		"include/IAKWrapper.h",
		"include/IncrementAndFreeze.h",
		"include/OSTCacheSim.h"
	],
	copts = [
		"-Iinclude",
		"-fopenmp"
	],
	linkopts = [
		"-lgomp",
	]
)

cc_library(
  name = "CacheSim",
  srcs = [
    "include/IncrementAndFreeze.h", 
    "include/OSTCacheSim.h",
    "include/OSTree.h",
    "include/IAKWrapper.h",
    "include/CacheSim.h",
    "include/params.h",
    "src/IncrementAndFreeze.cpp", 
    "src/OSTCacheSim.cpp",
    "src/OSTree.cpp",
    "src/IAKWrapper.cpp",
  ],
  hdrs = [
    "include/IncrementAndFreeze.h",
    "include/OSTCacheSim.h",
    "include/IAKWrapper.h",
    "include/CacheSim.h"
  ],
	copts = [
		"-fopenmp",
		"-Iinclude"
	],
	linkopts = [
		"-fopenmp"
	]
)

# Compile unit tests
cc_test(
  name = "unit_tests",
  size = "small",
  srcs = [
    "test/unit_tests.cpp", 
  ],
  includes = ["include/"],
  deps = [
    "@googletest//:gtest_main",
    "CacheSim"
  ],
)