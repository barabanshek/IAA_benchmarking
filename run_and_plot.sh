sudo ./build/iaa_bench --benchmark_repetitions=1 --benchmark_min_time=1x --benchmark_format=csv > tmp/results.csv
python3 plot.py tmp/results.csv $1
