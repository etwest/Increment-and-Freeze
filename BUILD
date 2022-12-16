test_suite(
    name = "tests",
    tests = ["unit_tests"],
)

cc_binary(
    name = "sim",
    deps = [
        ":iak_wrapper",
        ":ost_cache_sim",
        ":container_cache_sim",
    ],
    srcs = [
	"simulation.cc",
    ],
    linkopts = [
	"-lgomp",
    ]
)

cc_library(
    name = "cache_sim",
    hdrs = [
        "cache_sim.h",
        "sim_factory.h",
    ],
    deps = [
        "@absl//absl/time:time",
    ],
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
        ":params",
    ],
    copts = [
        "-fopenmp",
    ],
)

cc_library(
    name = "iak_wrapper",
    hdrs = ["iak_wrapper.h"],
    srcs = ["iak_wrapper.cc"],
    deps = [
        ":increment_and_freeze",
        ":params",
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
    ":iak_wrapper",
    ":ost_cache_sim",
    ":container_cache_sim",
  ],
  linkopts = [
      "-lgomp",
  ]
)
