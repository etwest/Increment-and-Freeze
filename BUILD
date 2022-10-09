test_suite(
    name = "tests",
    tests = ["unit_tests"],
)

cc_binary(
	name = "sim",
	deps = [
		"CacheSim",
                ":ost_cache_sim",
	],
	srcs = [
		"src/simulation.cpp",
		"include/IAKWrapper.h",
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
    name = "cache_sim",
    hdrs = ["cache_sim.h"],
)

cc_library(
    name = "params",
    hdrs = ["params.h"],
)

cc_library(
    name = "ostree",
    hdrs = ["ostree.h"],
    srcs = ["ostree.cc"],
)

cc_library(
    name = "ost_cache_sim",
    hdrs = ["ost_cache_sim.h"],
    srcs = ["ost_cache_sim.cc"],
    deps = [
        ":cache_sim",
        ":ostree",
    ],
)

cc_library(
    name = "increment_and_freeze",
    hdrs = ["increment_and_freeze.h"],
    srcs = ["increment_and_freeze.cc"],
    deps = [
        ":cache_sim",
        ":params",
    ],
    copts = [
	"-fopenmp",
    ],
)

cc_library(
  name = "CacheSim",
  srcs = [
    "include/IAKWrapper.h",
    "src/IAKWrapper.cpp",
  ],
  hdrs = [
    "include/IAKWrapper.h",
  ],
  deps = [
      ":cache_sim",
      ":params",
      ":increment_and_freeze",
      ":ostree",
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
    "CacheSim",
    ":ost_cache_sim",
  ],
)
