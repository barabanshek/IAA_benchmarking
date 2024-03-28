#!/usr/bin/env bash

repetitions=$1
name=$2
filter=$3

# Run benchmarks.
sudo ./build/iaa_bench --benchmark_repetitions=${repetitions} --benchmark_min_time=1x --benchmark_filter=${filter} --benchmark_format=csv --logtostderr | tee results.csv

# Plot results.
python3 plot_benchmark.py results.csv ${name}
