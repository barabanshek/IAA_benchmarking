#include <cstdarg>
#include <sys/mman.h>
#include <vector>

#include <iostream>

#include <benchmark/benchmark.h>

#include "qpl_compress_decompress.h"

#include "../util.h"
#include "benchmark.h"

namespace single_engine {

// {entropy, memory size} -> {ptr, true entropy}.
static std::map<std::tuple<uint16_t, size_t>, std::tuple<uint8_t *, double>>
    source_buffs;

void zero_initialize_counters(benchmark::State &state) {
  state.counters["Compression Time"] = 0;
  state.counters["Compression Ratio"] = 0;
}

auto BM_SingleEngineBlocking_Compress = [](benchmark::State &state,
                                           auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto execution_path = Inputs;
  auto compression_mode = va_arg(args, int);
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  zero_initialize_counters(state);

  // Benchmark compress.
  qpl_huffman_table_t huffman_tables;
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  for (auto _ : state) {
    if (single_engine::compress(
            execution_path, qpl_default_level,
            static_cast<single_engine::CompressionMode>(compression_mode),
            &huffman_tables, &last_bit_offset, source_buff, mem_size,
            compressed_buff, &compressed_size)) {
      LOG(FATAL) << "Failed to compress.";
      return;
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  size_t decompression_size = 0;
  if (single_engine::decompress(
          execution_path,
          static_cast<single_engine::CompressionMode>(compression_mode),
          huffman_tables, last_bit_offset, compressed_buff, compressed_size,
          decompressed_buff, mem_size, &decompression_size)) {
    LOG(FATAL) << "Failed to decompress.";
    return;
  }
  if (memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
    return;
  }

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

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  zero_initialize_counters(state);

  // Compress for verification.
  qpl_huffman_table_t huffman_tables;
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  if (single_engine::compress(
          execution_path, qpl_default_level,
          static_cast<single_engine::CompressionMode>(compression_mode),
          &huffman_tables, &last_bit_offset, source_buff, mem_size,
          compressed_buff, &compressed_size)) {
    LOG(FATAL) << "Failed to compress.";
    return;
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(decompressed_buff, 1, mem_size);

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(
            execution_path,
            static_cast<single_engine::CompressionMode>(compression_mode),
            huffman_tables, last_bit_offset, compressed_buff, compressed_size,
            decompressed_buff, mem_size, &decompression_size)) {
      LOG(FATAL) << "Failed to decompress.";
      return;
    }
  }

  // Verify.
  if (memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
    return;
  }

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

      auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      memset(compressed_buff, 1, mem_size);

      // Compress in software.
      auto compress_path = qpl_path_software;
      size_t compressed_size = 0;

      // Benchmark compress in software.
      zero_initialize_counters(state);
      TimeScope ts;
      qpl_huffman_table_t huffman_tables;
      if (single_engine::compress(compress_path, compression_level,
                                  single_engine::kModeDynamic, &huffman_tables,
                                  nullptr, source_buff, mem_size,
                                  compressed_buff, &compressed_size)) {
        LOG(FATAL) << "Failed to compress.";
        return;
      }
      state.counters["Compression Time"] =
          ts.GetTimeStamp<std::chrono::microseconds>();
      state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

      auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      memset(decompressed_buff, 1, mem_size);

      // Benchmark decompress in hardware.
      auto decompress_path = qpl_path_hardware;
      size_t decompression_size = 0;
      for (auto _ : state) {
        if (single_engine::decompress(
                decompress_path, single_engine::kModeDynamic, huffman_tables, 0,
                compressed_buff, compressed_size, decompressed_buff, mem_size,
                &decompression_size)) {
          LOG(FATAL) << "Failed to decompress.";
          return;
        }
      }

      // Verify.
      if (memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
        LOG(FATAL) << "Data missmatch.";
        return;
      }

      va_end(args);
      free(compressed_buff);
      free(decompressed_buff);
    };

void register_benchmarks() {
  // Register memory.
  // for (const auto entropy : {1, 5, 10, 25, 50, 150, 200, 300, 400}) {
  //   for (const size_t &mem_size : {512 * kMB, 256 * kMB, 64 * kMB, 16 * kMB,
  //                                  1 * kMB, 256 * kkB, 64 * kkB, 4 * kkB}) {
  for (const auto entropy : {1, 5}) {
    for (const size_t &mem_size : {64 * kMB, 16 * kMB}) {
      uint8_t *source_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      double true_entropy = init_rand_memory(source_buff, mem_size, entropy);
      source_buffs[std::make_tuple(entropy, mem_size)] =
          std::make_tuple(source_buff, true_entropy);
    }
  }

  for (const auto entropy : {1, 5}) {
    for (const size_t &mem_size : {64 * kMB, 16 * kMB}) {
      // for (const auto entropy : {1, 5, 10, 25, 50, 150, 200, 300, 400}) {
      //   for (const size_t &mem_size : {512 * kMB, 256 * kMB, 64 * kMB, 16 *
      //   kMB,
      //                                  1 * kMB, 256 * kkB, 64 * kkB, 4 *
      //                                  kkB}) {
      auto source_buff =
          std::get<0>(source_buffs[std::make_tuple(entropy, mem_size)]);
      auto true_entropy =
          std::get<1>(source_buffs[std::make_tuple(entropy, mem_size)]);

      for (const auto execution_path : {qpl_path_software, qpl_path_hardware}) {
        for (const auto compression_mode :
             {kModeStatic, kModeDynamic, kModeHuffmanOnly}) {
          // Compress.
          benchmark::RegisterBenchmark(
              "BM_SingleEngineBlocking_Compress_" +
                  std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                  std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                  +"_mode_" + std::to_string(compression_mode) +
                  (execution_path == qpl_path_software ? "_qpl_path_software"
                                                       : "_qpl_path_hardware"),
              BM_SingleEngineBlocking_Compress, execution_path,
              static_cast<int>(compression_mode), mem_size, source_buff);

          // Decompress.
          benchmark::RegisterBenchmark(
              "BM_SingleEngineBlocking_DeCompress_" +
                  std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                  std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                  +"_mode_" + std::to_string(compression_mode) +
                  +(execution_path == qpl_path_software ? "_qpl_path_software"
                                                        : "_qpl_path_hardware"),
              BM_SingleEngineBlocking_DeCompress, execution_path,
              static_cast<int>(compression_mode), mem_size, source_buff);
        }
      }
    }
  }

  // for (const auto entropy : {1, 5, 10, 25, 50, 150, 200, 300, 400}) {
  //   for (const size_t &mem_size : {512 * kMB, 256 * kMB, 64 * kMB, 16 * kMB,
  //                                  1 * kMB, 256 * kkB, 64 * kkB, 4 * kkB}) {
  //     auto source_buff =
  //         std::get<0>(source_buffs[std::make_tuple(entropy, mem_size)]);
  //     auto true_entropy =
  //         std::get<1>(source_buffs[std::make_tuple(entropy, mem_size)]);

  //     for (const auto compression_level : {qpl_level_1, qpl_level_3}) {
  //       benchmark::RegisterBenchmark(
  //           "BM_SingleEngineBlocking_SoftwareCompress_HardwareDecompress_" +
  //               std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
  //               std::to_string(entropy) + "_" + std::to_string(true_entropy)
  //               +
  //               "_level_" + std::to_string(compression_level),
  //           BM_SingleEngineBlocking_SoftwareCompress_HardwareDecompress,
  //           compression_level, mem_size, source_buff);
  //     }
  //   }
  // }
}

} // namespace single_engine
