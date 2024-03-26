# IAA Benchmarking

### Dependencies
* cmake
* [Google benchmark](https://github.com/google/benchmark), glog, gflags
* [nasm](https://www.nasm.us/)

### Run it
* clone the repo with submodules: `git clone --recursive https://github.com/barabanshek/IAA_benchmarking.git`
* build: `mkdir build; cd build; cmake ..; make -j; cd ..`
* get datasets: `cd dataset/; ./extract.sh; cd ..`
* configure hardware: `sudo ./configure_iaa_user.sh 0 1,7 16 8`
* run (all benchmarks) 3 time seach and collect data as .csv: `sudo ./build/iaa_bench --benchmark_repetitions=3 --benchmark_min_time=1x --benchmark_format=csv --logtostderr | tee tmp/results.csv`
* run specific benchmarks: `sudo ./build/iaa_bench --benchmark_repetitions=3 --benchmark_min_time=1x --benchmark_format=csv --benchmark_filter='.*MultipleEngine.*' --logtostderr | tee tmp/results.csv`
* plot all results: `python3 plot_corpus.py tmp/results.csv experiment`
