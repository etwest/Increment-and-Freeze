# Increment-and-Freeze
Increment-and-Freeze is a divide-and-conquer algorithm for computing the Success Function of a trace T.

## Requirements
- openmp
- bazel

## Experiments
Many experiment parameters are set in `params.h`.
Our experiments main file is `simulation.cc` and experiments are driven by the `run_simulations.sh` bash script.

## How to Use IAF
The Increment-and-Freeze algorithm is implmented in two libraries `increment_and_freeze` and `iak_wrapper`. d

### increment_and_freeze
This library implements the core IAF algorithm. The API to this algorithm and all other cache sims is defined in `cache_sim.h`. The key functions are:
- `memory_access(addr)`: Append a 64bit request id to the trace T.
- `get_success_function()`: Compute the success function of trace T. The success function is S(x) = number of hits in T at cache size x. The hit rate can be computed by dividing S(x) by the total number of accesses.

Additionally, some parameters to IAF are found in `params.h`.

### iak_wrapper
This library implements the online and universize aware IAF extension algorithm. Its API is identical to that of Increment-and-Freeze except that its constructor is as follows.
`IAKWrapper(min_chunk_size, cache_size_limit)`
- `min_chunk_size`: This optional parameter determines the minimum size of a chunk. Default = 64KiB.
- `cache_size_limit`: This optional parameter limits the number of values reported in the success function to be at most `cache_size_limit`.
