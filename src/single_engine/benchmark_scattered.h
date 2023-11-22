#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_scattered.h"

namespace single_engine_scattered {

auto BM_Scattered = [](benchmark::State &state, auto Inputs...) {
  // Parse input.
  va_list args;
  va_start(args, Inputs);
  size_t mem_size = Inputs;
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  // Set default counters.
  zero_initialize_counters(state);

  // Compress for verification.
  auto compressed_buff = mmap_allocate(mem_size);
  std::vector<size_t> compressed_sizes;
  size_t compressed_size = 0;
  if (single_engine_scattered::compress(source_buff, mem_size,
                                        compressed_buff.get(), &compressed_size,
                                        compressed_sizes))
    state.SkipWithMessage("Failed to compress.");
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;
  std::cout << state.counters["Compression Ratio"] << "\n";

  // Decompress scattered.
  auto decompressed_buff = mmap_allocate(2 * mem_size);
  memset(decompressed_buff.get(), 1, 2 * mem_size);
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine_scattered::decompress(
            compressed_buff.get(), compressed_sizes, decompressed_buff.get(),
            mem_size, &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
  return 0;
};

}
