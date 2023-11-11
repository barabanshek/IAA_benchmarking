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

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  //
  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Benchmark compress.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  for (auto _ : state) {
    if (single_engine::compress(
            execution_path, qpl_default_level,
            static_cast<single_engine::CompressionMode>(compression_mode),
            &huffman_table, &last_bit_offset, source_buff, mem_size,
            compressed_buff, &compressed_size)) {
      LOG(WARNING) << "Failed to compress.";
      continue;
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  if (single_engine::decompress(
          execution_path,
          static_cast<single_engine::CompressionMode>(compression_mode),
          huffman_table, last_bit_offset, compressed_buff, compressed_size,
          decompressed_buff, mem_size, &decompression_size)) {
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
  free(compressed_buff);
  free(decompressed_buff);
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

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  //
  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Compress for verification.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  if (single_engine::compress(
          execution_path, qpl_default_level,
          static_cast<single_engine::CompressionMode>(compression_mode),
          &huffman_table, &last_bit_offset, source_buff, mem_size,
          compressed_buff, &compressed_size)) {
    LOG(WARNING) << "Failed to compress.";
    for (auto _ : state) {
    }
    goto err;
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(decompressed_buff, 1, mem_size);

  // Benchmark decompress.
  for (auto _ : state) {
    if (single_engine::decompress(
            execution_path,
            static_cast<single_engine::CompressionMode>(compression_mode),
            huffman_table, last_bit_offset, compressed_buff, compressed_size,
            decompressed_buff, mem_size, &decompression_size)) {
      LOG(WARNING) << "Failed to decompress.";
      continue;
    }
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
  }
  state.counters["Status"] = 0;

err:
  va_end(args);
  free(compressed_buff);
  free(decompressed_buff);
};

auto BM_SingleEngineBlocking_SoftwareCompress_HardwareDecompress =
    [](benchmark::State &state, auto Inputs...) {
      va_list args;
      va_start(args, Inputs);
      auto compression_level = Inputs;
      auto mem_size = va_arg(args, size_t);
      uint8_t *source_buff = va_arg(args, uint8_t *);
      assert(source_buff != nullptr);

      auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(2 * mem_size));
      memset(compressed_buff, 1, mem_size);

      // Compress in software.
      auto compress_path = qpl_path_software;
      auto decompress_path = qpl_path_hardware;
      size_t compressed_size = 0;

      //
      uint8_t *decompressed_buff = nullptr;
      size_t decompression_size = 0;

      // Benchmark compress in software.
      zero_initialize_counters(state);
      TimeScope ts;
      qpl_huffman_table_t huffman_tables;
      uint32_t last_bit_offset = 0;
      if (single_engine::compress(compress_path, compression_level,
                                  single_engine::kModeDynamic, &huffman_tables,
                                  &last_bit_offset, source_buff, mem_size,
                                  compressed_buff, &compressed_size)) {
        LOG(WARNING) << "Failed to compress.";
        for (auto _ : state) {
        }
        goto err;
      }
      state.counters["Compression Time"] =
          ts.GetTimeStamp<std::chrono::microseconds>();
      state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

      decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      memset(decompressed_buff, 1, mem_size);

      // Benchmark decompress in hardware.
      for (auto _ : state) {
        if (single_engine::decompress(
                decompress_path, single_engine::kModeDynamic, huffman_tables,
                last_bit_offset, compressed_buff, compressed_size,
                decompressed_buff, mem_size, &decompression_size)) {
          LOG(WARNING) << "Failed to decompress.";
          continue;
        }
      }

      // Verify.
      if (decompression_size != mem_size ||
          memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
        LOG(FATAL) << "Data missmatch.";
      }
      state.counters["Status"] = 0;

    err:
      va_end(args);
      free(compressed_buff);
      free(decompressed_buff);
    };

auto BM_SingleEngineBlocking_CompressCanned = [](benchmark::State &state,
                                                 auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  const size_t chunk_size = 4 * kkB;

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  //
  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Benchmark compress.
  size_t compressed_size = 0;
  for (auto _ : state) {
    if (single_engine_canned::compress(
            static_cast<single_engine_canned::CompressionMode>(
                compression_mode),
            source_buff, mem_size, compressed_buff, &compressed_size,
            chunk_size)) {
      LOG(WARNING) << "Failed to compress.";
      continue;
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  if (single_engine_canned::decompress(compressed_buff, compressed_size,
                                       decompressed_buff, mem_size,
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

auto BM_SingleEngineBlocking_DeCompressCanned = [](benchmark::State &state,
                                                   auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto compression_mode = Inputs;
  auto mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  const size_t chunk_size = 4 * kkB;

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  //
  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Compress for verification.
  size_t compressed_size = 0;
  if (single_engine_canned::compress(
          static_cast<single_engine_canned::CompressionMode>(compression_mode),
          source_buff, mem_size, compressed_buff, &compressed_size,
          chunk_size)) {
    LOG(WARNING) << "Failed to compress.";
    for (auto _ : state) {
    }
    goto err;
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(decompressed_buff, 1, mem_size);

  // Benchmark decompress.
  for (auto _ : state) {
    if (single_engine_canned::decompress(compressed_buff, compressed_size,
                                         decompressed_buff, mem_size,
                                         &decompression_size)) {
      LOG(WARNING) << "Failed to decompress.";
      continue;
    }
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
  }
  state.counters["Status"] = 0;

err:
  va_end(args);
  free(decompressed_buff);
};

} // namespace single_engine

#endif
