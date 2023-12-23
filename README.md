# IAA Benchmarking

### Dependencies
* [Google benchmark](https://github.com/google/benchmark)
* Google Glog
* IAA (our own [fork](https://github.com/barabanshek/qpl/tree/microvm) as a submodule)

### Run it
* clone the repo with submodules: `git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git`
* get benchmarks: `cd dataset/; ./extract.sh; cd ..`
* run (all benchmarks) 3 time seach and collect data as .csv: `sudo ./build/iaa_bench --benchmark_repetitions=3 --benchmark_min_time=1x --benchmark_format=csv --logtostderr | tee tmp/results.csv`
* run specific benchmarks: `sudo ./build/iaa_bench --benchmark_repetitions=3 --benchmark_min_time=1x --benchmark_format=csv --benchmark_filter='.*MultipleEngine.*' --logtostderr | tee tmp/results.csv`
* plot all results: `python3 plot_corpus.py tmp/results.csv experiment`
