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
            source_buff, mem_size, &compressed_buff)) {
      LOG(WARNING) << "Failed to compress.";
      continue;
    }
  }
  size_t compressed_size = 0;
  for (auto cb_ : compressed_buff)
    compressed_size += std::get<0>(cb_).size();
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  uint8_t *decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  size_t decompression_size = 0;
  if (multi_engine::decompress(compressed_buff, decompressed_buff,
                               &decompression_size)) {
    LOG(WARNING) << "Failed to decompress.";
    goto err;
  }
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
    goto err;
  }
  state.counters["Status"] = 0;

err:
  va_end(args);
  free(decompressed_buff);
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

  size_t compressed_size = 0;
  uint8_t *decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(decompressed_buff, 1, mem_size);
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
    LOG(WARNING) << "Failed to compress.";
    for (auto _ : state) {
    }
    goto err;
  }
  for (auto cb_ : compressed_buff)
    compressed_size += std::get<0>(cb_).size();
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Decompress.
  for (auto _ : state) {
    if (multi_engine::decompress(compressed_buff, decompressed_buff,
                                 &decompression_size)) {
      LOG(WARNING) << "Failed to compress.";
      continue;
    }
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
    goto err;
  }
  state.counters["Status"] = 0;

err:
  va_end(args);
  free(decompressed_buff);
};
} // namespace multi_engine

#endif
