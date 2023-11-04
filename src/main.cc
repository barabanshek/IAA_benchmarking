#include <benchmark/benchmark.h>

#include <iostream>

#include "single_engine/benchmark.h"

int main(int argc, char **argv) {
  single_engine::register_benchmarks();

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
