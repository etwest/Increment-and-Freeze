
# $1 = location of parse_results.py
parse=$1

echo "," > results.csv

python3 $parse 1_threads_results/*

echo -n "PARDA - t1" >> results.csv
cat tmp_latency.csv >> results.csv
echo -n "PARDA - t1" >> results.csv
cat tmp_memory.csv >> results.csv
echo "," >> results.csv


python3 $parse 4_threads_results/*

echo -n "PARDA - t4" >> results.csv
cat tmp_latency.csv >> results.csv
echo -n "PARDA - t4" >> results.csv
cat tmp_memory.csv >> results.csv
echo "," >> results.csv


python3 $parse 8_threads_results/*

echo -n "PARDA - t8" >> results.csv
cat tmp_latency.csv >> results.csv
echo -n "PARDA - t8" >> results.csv
cat tmp_memory.csv >> results.csv
echo "," >> results.csv


python3 $parse 16_threads_results/*

echo -n "PARDA - t16" >> results.csv
cat tmp_latency.csv >> results.csv
echo -n "PARDA - t16" >> results.csv
cat tmp_memory.csv >> results.csv
echo "," >> results.csv


python3 $parse 48_threads_results/*

echo -n "PARDA - t48" >> results.csv
cat tmp_latency.csv >> results.csv
echo -n "PARDA - t48" >> results.csv
cat tmp_memory.csv >> results.csv
echo "," >> results.csv
