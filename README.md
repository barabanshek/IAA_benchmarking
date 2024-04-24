# IAA Benchmark

A set of benchmarks for Intel IAA compression/decompression (based on [QPL](https://intel.github.io/qpl) library) used in the paper ["Sabre: Improving Memory Prefetching in Serverless MicroVMs with Near-Memory Hardware-Accelerated Compression"]().

## Run with Docker
```
docker pull barabanshik/sabre_iaa_benchmark:latest
git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git

# Get datasets
pushd dataset/; ./extract.sh; popd

# Setup the machine (fix all CPU frequencies)
sudo ./prepare_machine.sh <freq, kHz>

# Configure hardware; make sure the script does NOT show errors after the line "Enabling Accelerators"
sudo ./configure_iaa_user.sh 0 <# of IAA devices> <WQ size> <# of engines per IAA device>

# Run benchmark, <N of iteration> must be > 1
./run_docker.sh <N of iteration> <benchmark filter regex>

# Examples:
./run_docker.sh 10 '.' # run all benchmarks, 10 times each
./run_docker.sh 10 '.*FullSystem.*' # run only full system benchmarks
./run_docker.sh 10 '.*SingleEngine.*' # run only single engine benchmarks
```

The results will be plotted and placed in `out/` folder. If custom changing in plotting is needed, the raw data will be at `out/results.csv`.

**KNOWN ISSUES:** As of now, Docker runs are not always stable; if fails - re-run again or use the manual run.

## Build locally and run manually

#### Dependencies
* all dependencies for [Intel QPL](https://intel.github.io/qpl/documentation/get_started_docs/installation.html#prerequisites)
* [cmake](https://cmake.org/) 3.16 or higer
* Python 3.11 or higher
* [idxd-config](https://github.com/intel/idxd-config) (for configuring IAA accelerators)
* [glog](https://github.com/google/glog), [gflags](https://github.com/gflags)

#### Build benchmarks
```
git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git
mkdir build; pushd build;
cmake ..

# Make sure "Build type: Release" for QPL

make -j;
popd
```

#### Run benchmarks and collect/plot results
Configure the machine as for [docker run](#run-with-docker).

Run (all benchmarks) N times each and collect data as a .csv file: 
```
sudo ./build/iaa_bench --benchmark_repetitions=<N> --benchmark_min_time=1x --benchmark_format=csv --logtostderr | tee results.csv
```
Run specific benchmarks N times (read [Google Benchmark documentation](https://github.com/google/benchmark) for details on filtering):
```
sudo ./build/iaa_bench --benchmark_repetitions=<N> --benchmark_min_time=1x --benchmark_format=csv --benchmark_filter='.*MultipleEngine.*' --logtostderr | tee results.csv
```
Verify benchmarks for errors and issues:
* make sure `stdout` does NOT contain line *"***WARNING*** Library was built as DEBUG. Timings may be affected."*
* `cat results.csv | grep false` will return any skipped/failed benchmark, idealy NONE
* on some hardware, some benchmarks might fail, we suggest to manually change the benchmark pass in `src/main.cc` and run this specific benchmark again.

Plot all results:
```
python3 plot_benchmark.py results.csv <prefix for the plot filenames>
```

**IMPORTANT:** Plot script is "quickly" written and is not configurable via cmdline args, please adjust manually if needed.

## APPENDIX: configuration used for the paper
* 1.7 GHz CPU: `./prepare_machine.sh 1700000`
* 4 IAA devices with 8 engines per device: `./configure_iaa_user.sh 0 1,7 16 8`
