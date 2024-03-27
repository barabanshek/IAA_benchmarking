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
  _PARSE_IN
  auto execution_path = Inputs;
  auto compression_mode = _PARSE_ARG(int);
  auto source_size = _PARSE_ARG(size_t);
  auto source_buff = _PARSE_ARG(uint8_t *);
  auto huffman_table = _PARSE_ARG(qpl_huffman_table_t);
  _PARSE_OUT

  assert(source_buff != nullptr);

  zero_initialize_counters(state);

  // Benchmark compress.
  size_t compressed_size =
      2 * source_size; // allocate initial space to fit even
                       // when compression increases data
  auto compressed_buff = malloc_allocate(compressed_size);
  memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  uint32_t last_bit_offset;
  for (auto _ : state) {
    if (single_engine::compress(
            execution_path, qpl_default_level,
            static_cast<single_engine::CompressionMode>(compression_mode),
            &huffman_table, &last_bit_offset, source_buff, source_size,
            compressed_buff.get(), &compressed_size))
      state.SkipWithMessage("Failed to compress.");
  }
  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(source_size);
  size_t decompression_size = 0;
  if (single_engine::decompress(
          execution_path,
          static_cast<single_engine::CompressionMode>(compression_mode),
          huffman_table, last_bit_offset, compressed_buff.get(),
          compressed_size, decompressed_buff.get(), source_size,
          &decompression_size))
    state.SkipWithMessage("Failed to decompress.");

  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_DeCompress = [](benchmark::State &state,
                                             auto Inputs...) {
  _PARSE_IN
  auto execution_path = Inputs;
  auto compression_mode = _PARSE_ARG(int);
  auto source_size = _PARSE_ARG(size_t);
  auto source_buff = _PARSE_ARG(uint8_t *);
  auto huffman_table = _PARSE_ARG(qpl_huffman_table_t);
  _PARSE_OUT

  assert(source_buff != nullptr);

  zero_initialize_counters(state);

  // Compress for verification.
  size_t compressed_size =
      2 * source_size; // allocate initial space to fit even
                       // when compression increases data
  auto compressed_buff = malloc_allocate(compressed_size);
  memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  uint32_t last_bit_offset;
  if (single_engine::compress(
          execution_path, qpl_default_level,
          static_cast<single_engine::CompressionMode>(compression_mode),
          &huffman_table, &last_bit_offset, source_buff, source_size,
          compressed_buff.get(), &compressed_size))
    state.SkipWithMessage("Failed to compress.");

  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  auto decompressed_buff = malloc_allocate(source_size);
  memset(decompressed_buff.get(), _PAGE_PREFAULT_, source_size);

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(
            execution_path,
            static_cast<single_engine::CompressionMode>(compression_mode),
            huffman_table, last_bit_offset, compressed_buff.get(),
            compressed_size, decompressed_buff.get(), source_size,
            &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_CompressCanned = [](benchmark::State &state,
                                                 auto Inputs...) {
  _PARSE_IN
  auto compression_mode = Inputs;
  auto source_size = _PARSE_ARG(size_t);
  auto source_buff = _PARSE_ARG(uint8_t *);
  _PARSE_OUT

  assert(source_buff != nullptr);

  const size_t chunk_size = 4 * kkB;

  zero_initialize_counters(state);

  // Benchmark compress.
  size_t compressed_size =
      2 * source_size; // allocate initial space to fit even
                       // when compression increases data
  auto compressed_buff = malloc_allocate(compressed_size);
  memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  qpl_huffman_table_t huffman_tables;
  for (auto _ : state) {
    if (single_engine_canned::compress(
            static_cast<single_engine_canned::CompressionMode>(
                compression_mode),
            source_buff, source_size, compressed_buff.get(), &compressed_size,
            chunk_size, &huffman_tables))
      state.SkipWithMessage("Failed to compress.");
  }
  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(source_size);
  size_t decompression_size = 0;
  if (single_engine_canned::decompress(compressed_buff.get(), compressed_size,
                                       decompressed_buff.get(), source_size,
                                       &decompression_size, huffman_tables))
    state.SkipWithMessage("Failed to decompress.");

  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

auto BM_SingleEngineBlocking_DeCompressCanned = [](benchmark::State &state,
                                                   auto Inputs...) {
  _PARSE_IN
  auto compression_mode = Inputs;
  auto source_size = _PARSE_ARG(size_t);
  auto source_buff = _PARSE_ARG(uint8_t *);
  _PARSE_OUT

  assert(source_buff != nullptr);

  const size_t chunk_size = 4 * kkB;

  zero_initialize_counters(state);

  // Compress for verification.
  size_t compressed_size =
      2 * source_size; // allocate initial space to fit even
                       // when compression increases data
  auto compressed_buff = malloc_allocate(compressed_size);
  memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  qpl_huffman_table_t huffman_tables;
  if (single_engine_canned::compress(
          static_cast<single_engine_canned::CompressionMode>(compression_mode),
          source_buff, source_size, compressed_buff.get(), &compressed_size,
          chunk_size, &huffman_tables))
    state.SkipWithMessage("Failed to compress.");

  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  auto decompressed_buff = malloc_allocate(source_size);
  memset(decompressed_buff.get(), _PAGE_PREFAULT_, source_size);

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine_canned::decompress(compressed_buff.get(), compressed_size,
                                         decompressed_buff.get(), source_size,
                                         &decompression_size, huffman_tables))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
};

} // namespace single_engine

#endif
