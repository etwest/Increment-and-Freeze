cc_library(
    name = "raw_order_statistic_set",
    hdrs = ["raw_order_statistic_set.h"],
    deps = [
        "@abseil-cpp//absl/memory",
    ],
)

cc_library(
    name = "raw_order_statistic_map",
    hdrs = ["raw_order_statistic_map.h"],
    deps = [
        ":raw_order_statistic_set",
    ],
)

cc_library(
    name = "order_statistic_set",
    hdrs = ["order_statistic_set.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":raw_order_statistic_set",
        "@glog//:glog",
    ],
)

cc_library(
    name = "order_statistic_test_common",
    testonly = 1,
    hdrs = ["order_statistic_test_common.h"],
    deps = [
        ":raw_order_statistic_set",
        "@abseil-cpp//absl/random",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "order_statistic_set_test",
    size = "small",
    srcs = ["order_statistic_set_test.cc"],
    tags = ["requires-net:loopback"],
    deps = [
        ":order_statistic_set",
        ":order_statistic_test_common",
        "@googletest//:gtest_main",
        "@abseil-cpp//absl/random",
        "@abseil-cpp//absl/strings",
    ],
    linkopts = [
	"-lgomp",
    ]
)
