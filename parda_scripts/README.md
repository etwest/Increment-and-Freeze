
# Comparison with PARDA
PARDA is a library for computing the LRU hit-rate curve in parallel. These scripts run our experiments on PARDA upon dump'd traces (see the dump_traces executable in IAF) and format the results into CSVs.

## PARDA Code
We downloaded PARDA from [Github](https://github.com/zixuanhe/parda) commit hash `60ad81cb930`.

## PARDA Paper Reference
Q. Niu, J. Dinan, Q. Lu and P. Sadayappan, "PARDA: A Fast Parallel Reuse Distance Analysis Algorithm," 2012 IEEE 26th International Parallel and Distributed Processing Symposium, Shanghai, China, 2012, pp. 1284-1294, doi: 10.1109/IPDPS.2012.117.

## Modifications
We made a few modifications to PARDA's source code in order to a fix bug and set our cache limits.
These are:
 - `parda.h:parda_generate_pfilename()` increase size of `pfilename` char buffer to 75 from 30. Avoids a segfault when filenames are too large.
 - `parda.h:DEFAULT_NBUCKETS` this parameter controls the cache limit used by PARDA. For our experiments in which we did not specify a cache limit we set this parameter to the number of unique addresses. For experiments in which we did specify a cache limit, then we set this parameter to the cache limit. 

## Instructions
`run_parda.sh`: This script runs parda and generates experiment outputs.  
`collect_results.sh`: This script parses the experiment results into CSVs.

Run `run_parda.sh <path/to/parda.x> <path/to/trace> <number of threads>`  

After running PARDA with 1, 4, 8, 16, and 48 threads (some of these can be commented out if desirded), run `collect_results.sh <path/to/parse_results.py>` to create the result CSV.

### Partitioning Traces
In order to support multi-threading, PARDA pre-partitions the source trace into a trace per thread. Therefore, before running a test on multiple threads the trace must be partitioned.

Run `partition_trace.sh <path_to_parda.x> <path/to/trace> <number of partitions to create> <number of lines in trace>`
