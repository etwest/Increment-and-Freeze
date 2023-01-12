#! /bin/bash
set -e

# Helper function to setup experiment
# $1 = name of experiments
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
# $2 = number of omp threads
zipfian_exprs() {
  (export OMP_NUM_THREADS=$2; ./bazel-bin/sim $1 zipfian 0.1)
  (export OMP_NUM_THREADS=$2; ./bazel-bin/sim $1 zipfian 0.2)
  (export OMP_NUM_THREADS=$2; ./bazel-bin/sim $1 zipfian 0.4)
  (export OMP_NUM_THREADS=$2; ./bazel-bin/sim $1 zipfian 0.6)
  (export OMP_NUM_THREADS=$2; ./bazel-bin/sim $1 zipfian 0.8)
}

# Reset experiment results file
echo "" > data+plots/tmp_expr_results.csv

# Run ContainerCacheSim experiments
setup_expr "ContainerCacheSim"
./bazel-bin/sim OS_SET uniform
zipfian_exprs OS_SET
finalize_expr

# Run IncrementAndFreeze experiments
setup_expr "IAF - t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim IAF uniform)
zipfian_exprs IAF 1
finalize_expr

setup_expr "IAF - t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim IAF uniform)
zipfian_exprs IAF 4
finalize_expr

setup_expr "IAF - t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim IAF uniform)
zipfian_exprs IAF 8
finalize_expr

setup_expr "IAF - t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim IAF uniform)
zipfian_exprs IAF 16
finalize_expr

setup_expr "IAF - t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim IAF uniform)
zipfian_exprs IAF 48
finalize_expr

# Run Chunk'd IncrementAndFreeze experiments
setup_expr "IAF Chunk'd - t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim CHUNK_IAF uniform)
zipfian_exprs CHUNK_IAF 1
finalize_expr

setup_expr "IAF Chunk'd - t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim CHUNK_IAF uniform)
zipfian_exprs CHUNK_IAF 4
finalize_expr

setup_expr "IAF Chunk'd - t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim CHUNK_IAF uniform)
zipfian_exprs CHUNK_IAF 8
finalize_expr

setup_expr "IAF Chunk'd - t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim CHUNK_IAF uniform)
zipfian_exprs CHUNK_IAF 16
finalize_expr

setup_expr "IAF Chunk'd - t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim CHUNK_IAF uniform)
zipfian_exprs CHUNK_IAF 48
finalize_expr

# Run Memory Limited Chunk'd IncrementAndFreeze experiments
setup_expr "Chunk'd - K_LIM - t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim K_LIM_IAF uniform)
zipfian_exprs K_LIM_IAF 1
finalize_expr

setup_expr "Chunk'd - K_LIM - t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim K_LIM_IAF uniform)
zipfian_exprs K_LIM_IAF 4
finalize_expr

setup_expr "Chunk'd - K_LIM - t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim K_LIM_IAF uniform)
zipfian_exprs K_LIM_IAF 8
finalize_expr

setup_expr "Chunk'd - K_LIM - t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim K_LIM_IAF uniform)
zipfian_exprs K_LIM_IAF 16
finalize_expr

setup_expr "Chunk'd - K_LIM - t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim K_LIM_IAF uniform)
zipfian_exprs K_LIM_IAF 48
finalize_expr
