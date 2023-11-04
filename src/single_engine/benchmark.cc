#include <sys/mman.h>
#include <vector>

#include <benchmark/benchmark.h>

#include "qpl_compress_decompress.h"

// static void BM_Malloc(benchmark::State &state) {
//   size_t mem_size = 256 * 1024 * 1024;
//   for (auto _ : state) {
//     volatile auto source_buff = reinterpret_cast<uint8_t
//     *>(malloc(mem_size));
//   }
// }

static void BM_SingleEngineBlocking_Compress(benchmark::State &state) {
  size_t mem_size = 256 * 1024 * 1024;
  auto source_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  memset(source_buff, 1, mem_size);
  memset(compressed_buff, 1, mem_size);

  size_t compressed_size = 0;
  for (auto _ : state) {
    if (qpl_single_engine::compress(source_buff, mem_size, compressed_buff,
                                    &compressed_size)) {
      LOG(FATAL) << "Failed to compress.";
      return;
    }
  }

  // Verify.
  auto decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  size_t decompression_size = 0;
  if (qpl_single_engine::decompress(compressed_buff, compressed_size,
                                    decompressed_buff, mem_size,
                                    &decompression_size)) {
    LOG(FATAL) << "Failed to decompress.";
    return;
  }
  if (memcmp(source_buff, decompressed_buff, decompression_size) != 0) {
    LOG(FATAL) << "Data missmatch.";
    return;
  }
}

// Register the function as a benchmark
BENCHMARK(BM_SingleEngineBlocking_Compress);

// BENCHMARK(BM_Malloc);
