#ifndef _MULTI_ENGINE_BENCHMARK_H_
#define _MULTI_ENGINE_BENCHMARK_H_

#include <cstdarg>

#include <glog/logging.h>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_parallel.h"

namespace multi_engine {

#define _PARSE_ARGS_                                                           \
  _PARSE_IN                                                                    \
  auto compression_mode = Inputs;                                              \
  auto mem_size = _PARSE_ARG(size_t);                                          \
  auto job_n = _PARSE_ARG(int);                                                \
  auto source_buff = _PARSE_ARG(uint8_t *);                                    \
  _PARSE_OUT

auto BM_MultipleEngine_Compress = [](benchmark::State &state, auto Inputs...) {
  _PARSE_ARGS_
  assert(source_buff != nullptr);

  size_t chunk_size = mem_size / static_cast<unsigned int>(job_n);
  size_t chunk_size_rem = mem_size % static_cast<unsigned int>(job_n);
  CompressedFormat compressed_buff;
  for (uint8_t i = 0; i < job_n; ++i) {
    if (chunk_size_rem && i == job_n - 1)
      chunk_size += chunk_size_rem;
    compressed_buff.push_back(std::make_tuple(
        std::vector<uint8_t>(2 * chunk_size, _PAGE_PREFAULT_),
        chunk_size)); // x2 space here to allow increase in compressed data
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
  _PARSE_ARGS_
  assert(source_buff != nullptr);

  size_t chunk_size = mem_size / static_cast<unsigned int>(job_n);
  size_t chunk_size_rem = mem_size % static_cast<unsigned int>(job_n);
  CompressedFormat compressed_buff;
  for (uint8_t i = 0; i < job_n; ++i) {
    if (chunk_size_rem && i == job_n - 1)
      chunk_size += chunk_size_rem;
    compressed_buff.push_back(std::make_tuple(
        std::vector<uint8_t>(2 * chunk_size, _PAGE_PREFAULT_),
        chunk_size)); // x2 space here to allow increase in compressed data
  }

  zero_initialize_counters(state);

  // Compress.
  size_t compressed_size = 0;
  if (multi_engine::compress(
          static_cast<multi_engine::CompressionMode>(compression_mode),
          source_buff, mem_size, &compressed_buff)) {
    state.SkipWithMessage("Failed to compress.");
  }
  for (auto cb_ : compressed_buff)
    compressed_size += std::get<0>(cb_).size();
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Decompress.
  auto decompressed_buff = mmap_allocate(mem_size);
  memset(decompressed_buff.get(), _PAGE_PREFAULT_, mem_size);
  size_t decompression_size = 0;
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
