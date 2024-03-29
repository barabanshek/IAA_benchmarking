#!/usr/bin/env bash

repetitions=$1
filter=$2

result_name="figure"

echo "Running IAA benchmarks for "${repetitions} "iterations; filter:" ${filter}

# Run benchmarks.
sudo LD_LIBRARY_PATH=/usr/local/lib ./build/iaa_bench --benchmark_repetitions=${repetitions} --benchmark_min_time=1x --benchmark_filter=${filter} --benchmark_format=csv --logtostderr | tee results.csv

# Plot results.
source /.venv/bin/activate
mkdir out
python3 plot_benchmark.py results.csv ${result_name}