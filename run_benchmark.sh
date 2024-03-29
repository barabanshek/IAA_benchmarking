#!/usr/bin/env bash

repetitions=$1
name=$2
filter=$3

echo "Running IAA benchmarks for "${repetitions} "iterations; save results under the name" ${name}"*; filter:" ${filter}

# Run benchmarks.
sudo LD_LIBRARY_PATH=/usr/local/lib ./build/iaa_bench --benchmark_repetitions=${repetitions} --benchmark_min_time=1x --benchmark_filter=${filter} --benchmark_format=csv --logtostderr | tee results.csv

# Plot results.
source /.venv/bin/activate
python3 plot_benchmark.py results.csv ${name}
