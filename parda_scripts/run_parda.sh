
set -e
set -u

# $1 - path to parda
# $2 - directory where the traces are
# $3 - number of threads to run

parda=$(readlink -f $1)
run_parda=$parda
trace_dir=$2
threads=$3

echo "Running Parda"
echo "Traces directory:  $trace_dir"
echo "Number of threads: $threads"

if (( $threads > 1 )); then
  run_parda="mpirun -oversubscribe -np $threads $parda --enable-mpi"
fi

# Move to trace directory and make output dir
cd $trace_dir
out_dir=$((threads))_threads_results

set +e
mkdir $out_dir
set -e
cd -

lines=$(wc -l < $trace_dir/uniform.trace)
bash partition_trace.sh $parda $trace_dir/uniform.trace $threads $lines

cd $trace_dir
echo "Running Uniform"
time $run_parda --input=uniform.trace --lines=$lines > $out_dir/seq_uniform.hist
cd -


lines=$(wc -l < $trace_dir/zipfian_0.1.trace)
bash partition_trace.sh $parda $trace_dir/zipfian_0.1.trace $threads $lines

cd $trace_dir
echo "Running Zipf 0.1"
time $run_parda --input=zipfian_0.1.trace --lines=$lines > $out_dir/seq_zipf_1.hist
cd -


lines=$(wc -l < $trace_dir/zipfian_0.2.trace)
bash partition_trace.sh $parda $trace_dir/zipfian_0.2.trace $threads $lines

cd $trace_dir
echo "Running Zipf 0.2"
time $run_parda --input=zipfian_0.2.trace --lines=$lines > $out_dir/seq_zipf_2.hist
cd -


lines=$(wc -l < $trace_dir/zipfian_0.4.trace)
bash partition_trace.sh $parda $trace_dir/zipfian_0.4.trace $threads $lines

cd $trace_dir
echo "Running Zipf 0.4"
time $run_parda --input=zipfian_0.4.trace --lines=$lines > $out_dir/seq_zipf_4.hist
cd -


lines=$(wc -l < $trace_dir/zipfian_0.6.trace)
bash partition_trace.sh $parda $trace_dir/zipfian_0.6.trace $threads $lines

cd $trace_dir
echo "Running Zipf 0.6"
time $run_parda --input=zipfian_0.6.trace --lines=$lines > $out_dir/seq_zipf_6.hist
cd -


lines=$(wc -l < $trace_dir/zipfian_0.8.trace)
bash partition_trace.sh $parda $trace_dir/zipfian_0.8.trace $threads $lines

cd $trace_dir
echo "Running Zipf 0.8"
time $run_parda --input=zipfian_0.8.trace --lines=$lines > $out_dir/seq_zipf_8.hist
cd -
