# Increment-and-Freeze
Increment-and-Freeze is a divide-and-conquer algorithm for computing the LRU success function (or hit-rate curve) of a trace T. It has log-fold parallelism and a cost of O(n/B log n) in the external memory model.

TODO: Paper reference here eventually

## Requirements
- openmp

## Experiments
Our experiments main file is `simulation.cc` and experiments are driven by the `run_simulations.sh` bash script. For the experiments reported in our paper we compiled our code with bazel configuration `fast32bit`.

The experiment parameters are set in `params.h`.

## How to Use IAF
The Increment-and-Freeze algorithm is implmented in two libraries `increment_and_freeze` and `bounded_iaf`.

### increment_and_freeze
This library implements the core IAF algorithm. The API to this algorithm and all other cache sims is defined in `cache_sim.h`. The key functions are:
- `memory_access(addr)`: Append a 64bit request id to the trace T.
- `get_success_function()`: Compute the success function of trace T. The success function is S(x) = number of hits in T at cache size x. The hit rate can be computed by dividing S(x) by the total number of accesses.
- `dump_success_function(fname, succ, sample_rate)`: Write the success function `succ` to the file `fname`. The `sample_rate`, that defaults to 1, controls how many cache sizes are reported in the success function. For example, if the sample rate is 2, then every other cache size is reported.

Additionally, some parameters to IAF are found in `iaf_params.h`. These are the basecase size and the fanout of the recursive tree.

### bounded_iaf
This library implements the online and universe size aware extension to the IAF algorithm. Its API is identical to that of Increment-and-Freeze except that its constructor is as follows.  
`IAKWrapper(min_chunk_size, cache_size_limit)`
- `min_chunk_size`: This optional parameter determines the minimum size of a chunk. Default = 64KiB.
- `cache_size_limit`: This optional parameter limits the number of values reported in the success function to be at most `cache_size_limit`. Limiting the number of values in the success function improves performance and reduces memory usages. So, it is recommended that a cache limit be provided if knowing the hit-rate of large cache sizes is unnecessary.

### Address Bits
By default our libraries use 64-bit integers in their datastructures. However, for a large portion of traces, 32-bit integers are sufficient. Passing `-DADDR_BIT32` when compiling the libraries will switch our datastructures to use 32-bit integers, improving runtime performance and halving memory consumption.
