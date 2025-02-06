test_suite(
    name = "tests",
    tests = ["unit_tests"],
)

cc_binary(
    name = "sim",
    deps = [
        ":bounded_iaf",
        ":ost_cache_sim",
        ":container_cache_sim",
    ],
    srcs = [
    	"simulation.cc",
        "params.h"
    ],
    linkopts = [
	   "-lgomp",
    ]
)

cc_binary(
    name = "dump_traces",
    deps = [
        ":cache_sim",
    ],
    srcs = [
        "dump_traces.cc",
        "params.h",
    ]
)

cc_library(
    name = "cache_sim",
    hdrs = [
        "cache_sim.h",
        "sim_factory.h",
    ],
    deps = [
        "@abseil-cpp//absl/time:time",
    ],
)

cc_library(
    name = "iaf_params",
    hdrs = ["iaf_params.h"],
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
    name = "container_cache_sim",
    hdrs = ["container_cache_sim.h"],
    srcs = ["container_cache_sim.cc"],
    deps = [
        ":cache_sim",
        "//container:order_statistic_set",
    ],
)

cc_library(
    name = "increment_and_freeze",
    hdrs = ["increment_and_freeze.h", "op.h", "partition.h", "projection.h"],
    srcs = ["increment_and_freeze.cc", "projection.cc"],
    deps = [
        ":cache_sim",
        ":iaf_params",
    ],
    copts = [
        "-fopenmp",
    ],
)

cc_library(
    name = "bounded_iaf",
    hdrs = ["bounded_iaf.h"],
    srcs = ["bounded_iaf.cc"],
    deps = [
        ":increment_and_freeze",
        ":iaf_params",
    ],
)

# Compile unit tests
cc_test(
  name = "unit_tests",
  size = "small",
  srcs = [
        "unit_tests.cc",
        "memory_cutoff_tests.cc"
  ],
  deps = [
    "@googletest//:gtest_main",
    ":bounded_iaf",
    ":ost_cache_sim",
    ":container_cache_sim",
  ],
  linkopts = [
      "-lgomp",
  ]
)
