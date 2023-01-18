
set -u
set -e
# Partitions a trace into seperate files using parda
# $1 - path to parda.x
# $2 - path to trace to partition
# $3 - number of partitioned files to create (number of threads)
# $4 - number of lines in trace

parda=$(readlink -f $1)
trace_dir=$(dirname $2)
trace=$(basename $2)
threads=$3
lines=$4

echo "partitioning $trace into $threads files . . ."

if (( $threads < 2 )); then
  echo "NO OP"
  exit 0
fi

cd $trace_dir
$parda --enable-seperate --input=$trace --lines=$lines --threads=$threads
cd -

echo "Done!"
