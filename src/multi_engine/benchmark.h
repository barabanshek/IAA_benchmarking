#ifndef _MULTI_ENGINE_BENCHMARK_H_
#define _MULTI_ENGINE_BENCHMARK_H_

#include <cstdarg>

#include <glog/logging.h>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_parallel.h"

namespace multi_engine {

auto BM_MultipleEngine_Compress = [](benchmark::State &state, auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  size_t mem_size = va_arg(args, size_t);
  int job_n = va_arg(args, int);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  size_t chunk_size = mem_size / static_cast<unsigned int>(job_n);
  size_t chunk_size_rem = mem_size % static_cast<unsigned int>(job_n);
  CompressedFormat compressed_buff;
  for (uint8_t i = 0; i < job_n; ++i) {
    if (chunk_size_rem && i == job_n - 1)
      chunk_size += chunk_size_rem;
    compressed_buff.push_back(
        std::make_tuple(std::vector<uint8_t>(chunk_size, 1), chunk_size));
  }

  zero_initialize_counters(state);

  // Benchmark compress.
  for (auto _ : state) {
    if (multi_engine::compress(
            static_cast<multi_engine::CompressionMode>(compression_mode),
            source_buff, mem_size, &compressed_buff))
      state.SkipWithMessage("Failed to compress.");
  }
  size_t compressed_size = 0;
  for (auto cb_ : compressed_buff)
    compressed_size += std::get<0>(cb_).size();
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = mmap_allocate(mem_size);
  size_t decompression_size = 0;
  if (multi_engine::decompress(compressed_buff, decompressed_buff.get(),
                               &decompression_size))
    state.SkipWithMessage("Failed to decompress.");
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_MultipleEngine_DeCompress = [](benchmark::State &state,
                                       auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  size_t mem_size = va_arg(args, size_t);
  int job_n = va_arg(args, int);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  size_t compressed_size = 0;
  auto decompressed_buff = mmap_allocate(mem_size);
  memset(decompressed_buff.get(), 1, mem_size);
  size_t decompression_size = 0;

  size_t chunk_size = mem_size / static_cast<unsigned int>(job_n);
  size_t chunk_size_rem = mem_size % static_cast<unsigned int>(job_n);
  CompressedFormat compressed_buff;
  for (uint8_t i = 0; i < job_n; ++i) {
    if (chunk_size_rem && i == job_n - 1)
      chunk_size += chunk_size_rem;
    compressed_buff.push_back(
        std::make_tuple(std::vector<uint8_t>(chunk_size, 1), chunk_size));
  }

  zero_initialize_counters(state);

  // Compress.
  if (multi_engine::compress(
          static_cast<multi_engine::CompressionMode>(compression_mode),
          source_buff, mem_size, &compressed_buff)) {
    state.SkipWithMessage("Failed to compress.");
  }
  for (auto cb_ : compressed_buff)
    compressed_size += std::get<0>(cb_).size();
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Decompress.
  for (auto _ : state) {
    if (multi_engine::decompress(compressed_buff, decompressed_buff.get(),
                                 &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }
  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

} // namespace multi_engine

#endif
