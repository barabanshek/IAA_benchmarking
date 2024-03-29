# IAA Benchmark

A set of benchmarks for Intel IAA compression/decompression (based on [QPL](https://intel.github.io/qpl) library) used in the paper ["Sabre: Improving Memory Prefetching in Serverless MicroVMs with Near-Memory Hardware-Accelerated Compression"]().

## Run with Docker
* get docker image: `docker pull barabanshik/sabre_iaa_benchmark:latest`
* clone the repo with submodules: `git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git`
* get datasets: `cd dataset/; ./extract.sh; cd ..`
* setup the machine (fix all CPU frequencies): `./prepare_machine.sh <freq, kHz>`
* configure hardware: `sudo ./configure_iaa_user.sh 0 <# of IAA devices> <WQ size> <# of engines per IAA device>`, make sure the script does NOT show errors after the line *"Enabling Accelerators"*
    * verify hardware configuration: `sudo accel-config list`, please, read [documentation](https://github.com/intel/idxd-config/blob/stable/Documentation/accfg/accel-config-list.txt) for interpretation
* run benchmark: `./run_docker.sh <N of iteration> <benchmark filter regex>`, `<N of iteration>` must be > 1
    * example: `./run_docker.sh 10 '.'` - run all benchmarks, 10 times each
    * example: `./run_docker.sh 10 '.*FullSystem.*'` - run only full system benchmarks
    * example: `./run_docker.sh 10 '.*SingleEngine.*'` - run only single engine benchmarks
* the results will be plotted and placed in `out/` folder
* if custom changing in plotting is needed, the raw data will also be in `out/`

## Build locally and run manually

#### Dependencies
* all dependencies for [Intel QPL](https://intel.github.io/qpl/documentation/get_started_docs/installation.html#prerequisites)
* [cmake](https://cmake.org/) 3.16 or higer
* Python 3.11 or higher
* [idxd-config](https://github.com/intel/idxd-config) (for configuring IAA accelerators)
* [glog](https://github.com/google/glog), [gflags](https://github.com/gflags)

#### Build benchmarks
* clone the repo with submodules: `git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git`
* `mkdir build; cd build;`
* `cmake ..`
    * make sure *"Build type: Release"* for QPL
* `make -j;`
* `cd ..`

#### Run benchmarks and collect/plot results
* configure the machine as for [docker run](#run-with-docker)
* run (all benchmarks) N times each and collect data as a .csv file: `sudo ./build/iaa_bench --benchmark_repetitions=<N> --benchmark_min_time=1x --benchmark_format=csv --logtostderr | tee results.csv`
* run specific benchmarks N times: `sudo ./build/iaa_bench --benchmark_repetitions=<N> --benchmark_min_time=1x --benchmark_format=csv --benchmark_filter='.*MultipleEngine.*' --logtostderr | tee results.csv`, read [Google Benchmark documentation](https://github.com/google/benchmark) for details on filtering.
* verify benchmarks for errors and issues:
    * make sure `stdout` does NOT contain line *"***WARNING*** Library was built as DEBUG. Timings may be affected."*
    * `cat tmp/results.csv | grep false` will return any skipped/failed benchmark, idealy NONE
    * on some hardware, some benchmarks might fail, we suggest to manually change the benchmark pass in `src/main.cc` and run this specific benchmark again; Example of a failed benchmark:
        * "BM_MultipleEngine_DeCompress_21100kB_name_dataset/silesia_tmp/samba_entropy_0.025140_jobs_22_mode_2",,,,,,,,false,"Failed to compress."
        * re-run with: `sudo ./build/iaa_bench --benchmark_repetitions=1 --benchmark_min_time=1x --benchmark_filter='.*BM_MultipleEngine_Compress_21100kB_name_dataset/silesia_tmp/samba_entropy_0.025140_jobs_26_mode_2.*' --benchmark_format=csv --logtostderr`
* plot all results: `python3 plot_benchmark.py results.csv <prefix for the plot filenames>` (script will SKIP data from failed benchmarks)
    * plot script is "quickly" written and is not configurable via cmdline args, please adjust manually if needed

## APPENDIX: configuration used for the paper
* 1.7 GHz CPU: `./prepare_machine.sh 1700000`
* 4 IAA devices with 8 engines per device: `./configure_iaa_user.sh 0 1,7 16 8`
