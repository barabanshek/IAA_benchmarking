#include <cstdarg>
#include <sys/mman.h>
#include <vector>

#include <iostream>

#include <benchmark/benchmark.h>

#include "qpl_compress_decompress.h"

#include "../util.h"
#include "benchmark.h"

namespace single_engine {

static std::map<std::tuple<uint16_t, size_t>, uint8_t *> source_buffs;

auto BM_SingleEngineBlocking_Compress = [](benchmark::State &state,
                                           auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto execution_path = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  size_t compressed_size = 0;
  for (auto _ : state) {
    if (single_engine::compress(execution_path, source_buff, mem_size,
                                compressed_buff, &compressed_size)) {
      LOG(FATAL) << "Failed to compress.";
      return;
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify.
  auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  size_t decompression_size = 0;
  if (single_engine::decompress(execution_path, compressed_buff,
                                compressed_size, decompressed_buff, mem_size,
                                &decompression_size)) {
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
  auto mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(compressed_buff, 1, mem_size);

  size_t compressed_size = 0;
  if (single_engine::compress(execution_path, source_buff, mem_size,
                              compressed_buff, &compressed_size)) {
    LOG(FATAL) << "Failed to compress.";
    return;
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(decompressed_buff, 1, mem_size);

  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(execution_path, compressed_buff,
                                  compressed_size, decompressed_buff, mem_size,
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
  for (const auto entropy : {1, 5, 10, 25, 50, 150, 200, 300, 400}) {
    for (const size_t &mem_size : {512 * kMB, 256 * kMB, 64 * kMB, 16 * kMB,
                                   1 * kMB, 256 * kkB, 64 * kkB, 4 * kkB}) {
      uint8_t *source_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      double true_entropy = init_rand_memory(source_buff, mem_size, entropy);
      source_buffs[std::make_tuple(entropy, mem_size)] = source_buff;

      for (const auto execution_path : {qpl_path_software, qpl_path_hardware}) {
        // Compress.
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_Compress_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                (execution_path == qpl_path_software ? "_qpl_path_software"
                                                     : "_qpl_path_hardware"),
            BM_SingleEngineBlocking_Compress, execution_path, mem_size,
            source_buff);

        // Decompress.
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_DeCompress_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                +(execution_path == qpl_path_software ? "_qpl_path_software"
                                                      : "_qpl_path_hardware"),
            BM_SingleEngineBlocking_DeCompress, execution_path, mem_size,
            source_buff);
      }
    }
  }
}

} // namespace single_engine
