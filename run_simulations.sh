#! /bin/bash
set -e

# Helper function to setup experiment
# $1 = name of cache sim
setup_expr() {
  echo "Running $1 experiments"
  echo -n "$1, latency" > tmp_latency.csv
  echo -n "$1, memory(MIB)" > tmp_memory.csv
}

# Helper function for finalizing experiment
finalize_expr() {
  cat tmp_latency.csv >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv
  cat tmp_memory.csv >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv
}

# Helper function to run zipfian experiments
# $1 = sim_enum
zipfian_exprs() {
  ./bazel-bin/sim $1 zipfian 0.1
  ./bazel-bin/sim $1 zipfian 0.2
  ./bazel-bin/sim $1 zipfian 0.4
  ./bazel-bin/sim $1 zipfian 0.6
  ./bazel-bin/sim $1 zipfian 0.8
}

# Reset experiment results file
echo "" > data+plots/tmp_expr_results.csv

# Run OrderStatisticTree experiments
# setup_expr "OrderStatisticTree"
# ./bazel-bin/sim OS_TREE uniform
# zipfian_exprs OS_TREE
# finalize_expr

# Run ContainerCacheSim experiments
# setup_expr "ContainerCacheSim"
# ./bazel-bin/sim OS_SET uniform
# zipfian_exprs OS_SET
# finalize_expr

# Run IncrementAndFreeze experiments
setup_expr "IAF"
./bazel-bin/sim IAF uniform
zipfian_exprs IAF
finalize_expr

# Run Chunk'd IncrementAndFreeze experiments
setup_expr "IAF Chunk'd"
./bazel-bin/sim CHUNK_IAF uniform
zipfian_exprs CHUNK_IAF
finalize_expr

# Run Memory Limited Chunk'd IncrementAndFreeze experiments
setup_expr "Chunk'd - K_LIM"
./bazel-bin/sim K_LIM_IAF uniform
zipfian_exprs K_LIM_IAF
finalize_expr

