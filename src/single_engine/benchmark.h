#ifndef _SINGLE_ENGINE_BENCHMARK_H_
#define _SINGLE_ENGINE_BENCHMARK_H_

#include <cmath>
#include <cstdarg>
#include <sys/mman.h>
#include <vector>

#include <iostream>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_canned.h"
#include "qpl_compress_decompress.h"

namespace single_engine {

auto BM_SingleEngineBlocking_Compress = [](benchmark::State &state,
                                           auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto execution_path = Inputs;
  auto compression_mode = va_arg(args, int);
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  qpl_huffman_table_t huffman_table = va_arg(args, qpl_huffman_table_t);
  va_end(args);

  auto compressed_buff = malloc_allocate(2 * mem_size);
  memset(compressed_buff.get(), 1, 2 * mem_size);

  zero_initialize_counters(state);

  // Benchmark compress.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  for (auto _ : state) {
    if (single_engine::compress(
            execution_path, qpl_default_level,
            static_cast<single_engine::CompressionMode>(compression_mode),
            &huffman_table, &last_bit_offset, source_buff, mem_size,
            compressed_buff.get(), &compressed_size))
      state.SkipWithMessage("Failed to compress.");
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(mem_size);
  size_t decompression_size = 0;
  if (single_engine::decompress(
          execution_path,
          static_cast<single_engine::CompressionMode>(compression_mode),
          huffman_table, last_bit_offset, compressed_buff.get(),
          compressed_size, decompressed_buff.get(), mem_size,
          &decompression_size))
    state.SkipWithMessage("Failed to decompress.");

  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_DeCompress = [](benchmark::State &state,
                                             auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto execution_path = Inputs;
  auto compression_mode = va_arg(args, int);
  auto mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  qpl_huffman_table_t huffman_table = va_arg(args, qpl_huffman_table_t);
  va_end(args);

  auto compressed_buff = malloc_allocate(2 * mem_size);
  memset(compressed_buff.get(), 1, 2 * mem_size);

  zero_initialize_counters(state);

  // Compress for verification.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  if (single_engine::compress(
          execution_path, qpl_default_level,
          static_cast<single_engine::CompressionMode>(compression_mode),
          &huffman_table, &last_bit_offset, source_buff, mem_size,
          compressed_buff.get(), &compressed_size))
    state.SkipWithMessage("Failed to compress.");

  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  auto decompressed_buff = malloc_allocate(mem_size);
  memset(decompressed_buff.get(), 1, mem_size);

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(
            execution_path,
            static_cast<single_engine::CompressionMode>(compression_mode),
            huffman_table, last_bit_offset, compressed_buff.get(),
            compressed_size, decompressed_buff.get(), mem_size,
            &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_SoftwareCompress_HardwareDecompress =
    [](benchmark::State &state, auto Inputs...) {
      va_list args;
      va_start(args, Inputs);
      auto compression_level = Inputs;
      auto mem_size = va_arg(args, size_t);
      uint8_t *source_buff = va_arg(args, uint8_t *);
      assert(source_buff != nullptr);
      va_end(args);

      auto compressed_buff = malloc_allocate(2 * mem_size);
      memset(compressed_buff.get(), 1, 2 * mem_size);

      // Compress in software.
      auto compress_path = qpl_path_software;
      auto decompress_path = qpl_path_hardware;
      size_t compressed_size = 0;

      // Benchmark compress in software.
      zero_initialize_counters(state);
      TimeScope ts;
      qpl_huffman_table_t huffman_tables;
      uint32_t last_bit_offset = 0;
      if (single_engine::compress(compress_path, compression_level,
                                  single_engine::kModeDynamic, &huffman_tables,
                                  &last_bit_offset, source_buff, mem_size,
                                  compressed_buff.get(), &compressed_size))
        state.SkipWithMessage("Failed to compress.");

      state.counters["Compression Time"] =
          ts.GetTimeStamp<std::chrono::microseconds>();
      state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

      auto decompressed_buff = malloc_allocate(mem_size);
      memset(decompressed_buff.get(), 1, mem_size);

      // Benchmark decompress in hardware.
      size_t decompression_size = 0;
      for (auto _ : state) {
        if (single_engine::decompress(
                decompress_path, single_engine::kModeDynamic, huffman_tables,
                last_bit_offset, compressed_buff.get(), compressed_size,
                decompressed_buff.get(), mem_size, &decompression_size))
          state.SkipWithMessage("Failed to decompress.");
      }

      // Verify.
      if (decompression_size != mem_size ||
          memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
        state.SkipWithMessage("Data missmatch.");

      state.counters["Status"] = 0;
    };

auto BM_SingleEngineBlocking_CompressCanned = [](benchmark::State &state,
                                                 auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  const size_t chunk_size = 4 * kkB;

  auto compressed_buff = malloc_allocate(mem_size);
  memset(compressed_buff.get(), 1, mem_size);

  zero_initialize_counters(state);

  // Benchmark compress.
  size_t compressed_size = 0;
  qpl_huffman_table_t huffman_tables;
  for (auto _ : state) {
    if (single_engine_canned::compress(
            static_cast<single_engine_canned::CompressionMode>(
                compression_mode),
            source_buff, mem_size, compressed_buff.get(), &compressed_size,
            chunk_size, &huffman_tables))
      state.SkipWithMessage("Failed to compress.");
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(mem_size);
  size_t decompression_size = 0;
  if (single_engine_canned::decompress(compressed_buff.get(), compressed_size,
                                       decompressed_buff.get(), mem_size,
                                       &decompression_size, huffman_tables))
    state.SkipWithMessage("Failed to decompress.");

  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_DeCompressCanned = [](benchmark::State &state,
                                                   auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  auto mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  const size_t chunk_size = 4 * kkB;

  auto compressed_buff = malloc_allocate(mem_size);
  memset(compressed_buff.get(), 1, mem_size);

  zero_initialize_counters(state);

  // Compress for verification.
  size_t compressed_size = 0;
  qpl_huffman_table_t huffman_tables;
  if (single_engine_canned::compress(
          static_cast<single_engine_canned::CompressionMode>(compression_mode),
          source_buff, mem_size, compressed_buff.get(), &compressed_size,
          chunk_size, &huffman_tables))
    state.SkipWithMessage("Failed to compress.");

  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  auto decompressed_buff = malloc_allocate(mem_size);
  memset(decompressed_buff.get(), 1, mem_size);

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine_canned::decompress(compressed_buff.get(), compressed_size,
                                         decompressed_buff.get(), mem_size,
                                         &decompression_size, huffman_tables))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

} // namespace single_engine

#endif
