#! /bin/bash
set -e

# Helper function to setup experiment
# $1 = name of experiments
setup_expr() {
  echo "Running $1 experiments"
  echo -n "$1, latency" > tmp_latency.csv
  echo -n "$1, memory(MIB)" > tmp_memory.csv

  set +e
  mkdir data+plots/$1_hist
  set -e
}

# Helper function for finalizing experiment
# $1 = name of experiments
finalize_expr() {
  cat tmp_latency.csv >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv
  cat tmp_memory.csv >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv
  echo "" >> data+plots/tmp_expr_results.csv

  mv *.hist data+plots/$1_hist/
}

# Helper function to run zipfian experiments
# $1 = sim_enum
zipfian_exprs() {
  ./bazel-bin/sim zipf_0.1.hist $1 zipfian 0.1
  ./bazel-bin/sim zipf_0.2.hist $1 zipfian 0.2
  ./bazel-bin/sim zipf_0.4.hist $1 zipfian 0.4
  ./bazel-bin/sim zipf_0.6.hist $1 zipfian 0.6
  ./bazel-bin/sim zipf_0.8.hist $1 zipfian 0.8
}

# Helper function to validate correctness as
# compared to ContainerCacheSim
validate() {
  sum=0
  diff data+plots/ContainerCacheSim_hist/uniform.hist data+plots/$1/uniform.hist
  sum=$((sum+$?))
  diff data+plots/ContainerCacheSim_hist/zipf_0.1.hist data+plots/$1/zipf_0.1.hist
  sum=$((sum+$?))
  diff data+plots/ContainerCacheSim_hist/zipf_0.2.hist data+plots/$1/zipf_0.2.hist
  sum=$((sum+$?))
  diff data+plots/ContainerCacheSim_hist/zipf_0.4.hist data+plots/$1/zipf_0.4.hist
  sum=$((sum+$?))
  diff data+plots/ContainerCacheSim_hist/zipf_0.6.hist data+plots/$1/zipf_0.6.hist
  sum=$((sum+$?))
  diff data+plots/ContainerCacheSim_hist/zipf_0.8.hist data+plots/$1/zipf_0.8.hist
  sum=$((sum+$?))

  if ((sum == 0)); then
    echo "Validated! $1"
  else
    echo "ERROR: Histograms do not match! $1"
  fi
}

# Reset experiment results file
echo "" > data+plots/tmp_expr_results.csv

# Run ContainerCacheSim experiments
setup_expr "ContainerCacheSim"
./bazel-bin/sim uniform.hist OS_SET uniform
zipfian_exprs OS_SET
finalize_expr "ContainerCacheSim"

# Run IncrementAndFreeze experiments
setup_expr "IAF-t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim uniform.hist IAF uniform)
(export OMP_NUM_THREADS=1; zipfian_exprs IAF 1)
finalize_expr "IAF-t1"

setup_expr "IAF-t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim uniform.hist IAF uniform)
(export OMP_NUM_THREADS=4; zipfian_exprs IAF 4)
finalize_expr "IAF-t4"

setup_expr "IAF-t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim uniform.hist IAF uniform)
(export OMP_NUM_THREADS=8; zipfian_exprs IAF 8)
finalize_expr "IAF-t8"

setup_expr "IAF-t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim uniform.hist IAF uniform)
(export OMP_NUM_THREADS=16; zipfian_exprs IAF 16)
finalize_expr "IAF-t16"

setup_expr "IAF-t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim uniform.hist IAF uniform)
(export OMP_NUM_THREADS=48; zipfian_exprs IAF 48)
finalize_expr "IAF-t48"

# Run BoundedIncrementAndFreeze experiments
setup_expr "BoundedIAF-t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim uniform.hist BOUND_IAF uniform)
(export OMP_NUM_THREADS=1; zipfian_exprs BOUND_IAF 1)
finalize_expr "BoundedIAF-t1"

setup_expr "BoundedIAF-t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim uniform.hist BOUND_IAF uniform)
(export OMP_NUM_THREADS=4; zipfian_exprs BOUND_IAF 4)
finalize_expr "BoundedIAF-t4"

setup_expr "BoundedIAF-t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim uniform.hist BOUND_IAF uniform)
(export OMP_NUM_THREADS=8; zipfian_exprs BOUND_IAF 8)
finalize_expr "BoundedIAF-t8"

setup_expr "BoundedIAF-t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim uniform.hist BOUND_IAF uniform)
(export OMP_NUM_THREADS=16; zipfian_exprs BOUND_IAF 16)
finalize_expr "BoundedIAF-t16"

setup_expr "BoundedIAF-t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim uniform.hist BOUND_IAF uniform)
(export OMP_NUM_THREADS=48; zipfian_exprs BOUND_IAF 48)
finalize_expr "BoundedIAF-t48"

# Run Memory Limited Chunk IncrementAndFreeze experiments
setup_expr "Bounded_K_LIM-t1"
(export OMP_NUM_THREADS=1; ./bazel-bin/sim uniform.hist K_LIM_IAF uniform)
(export OMP_NUM_THREADS=1; zipfian_exprs K_LIM_IAF 1)
finalize_expr "Bounded_K_LIM-t1"

setup_expr "Bounded_K_LIM-t4"
(export OMP_NUM_THREADS=4; ./bazel-bin/sim uniform.hist K_LIM_IAF uniform)
(export OMP_NUM_THREADS=4; zipfian_exprs K_LIM_IAF 4)
finalize_expr "Bounded_K_LIM-t4"

setup_expr "Bounded_K_LIM-t8"
(export OMP_NUM_THREADS=8; ./bazel-bin/sim uniform.hist K_LIM_IAF uniform)
(export OMP_NUM_THREADS=8; zipfian_exprs K_LIM_IAF 8)
finalize_expr "Bounded_K_LIM-t8"

setup_expr "Bounded_K_LIM-t16"
(export OMP_NUM_THREADS=16; ./bazel-bin/sim uniform.hist K_LIM_IAF uniform)
(export OMP_NUM_THREADS=16; zipfian_exprs K_LIM_IAF 16)
finalize_expr "Bounded_K_LIM-t16"

setup_expr "Bounded_K_LIM-t48"
(export OMP_NUM_THREADS=48; ./bazel-bin/sim uniform.hist K_LIM_IAF uniform)
(export OMP_NUM_THREADS=48; zipfian_exprs K_LIM_IAF 48)
finalize_expr "Bounded_K_LIM-t48"

# Validate correctness
if (( $# == 1)) && [ "$1" = "validate" ]; then
  validate IAF-t1_hist
  validate IAF-t4_hist
  validate IAF-t8_hist
  validate IAF-t16_hist
  validate IAF-t48_hist
  validate BoundedIAF-t1_hist
  validate BoundedIAF-t4_hist
  validate BoundedIAF-t8_hist
  validate BoundedIAF-t16_hist
  validate BoundedIAF-t48_hist
fi
