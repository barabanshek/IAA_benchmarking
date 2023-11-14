#ifndef _BENCHMARK_PAGE_FAULTS_H_
#define _BENCHMARK_PAGE_FAULTS_H_

#include <cmath>
#include <cstdarg>
#include <iostream>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "iaa_utils.h"
#include "qpl_canned.h"
#include "qpl_compress_decompress.h"

namespace page_faults {

enum PageFaultScenario {
  kMajorPageFaults,
  kMinorPageFaults,
  kAtsMiss,
  kNoFaults
};

/// Re-mmap memory @param buff from a file to allow major page faults later (if
/// @param prefault = false).
static int remmap_memory(uint8_t **buff, size_t size, const char *filename,
                         bool drop_cache, bool prefault,
                         bool free_old_buff = true) {
  // Dump source buffer to file for major page faults.
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0x666);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return -1;
  }
  size_t s = write(fd, *buff, size);
  if (s != size) {
    LOG(WARNING) << "Failed to write pf file.";
    return -1;
  }
  fsync(fd);
  close(fd);

  if (free_old_buff)
    free(*buff);

  // Drop caches.
  if (drop_cache) {
    int res = system((std::string("sudo dd of=") + filename +
                      " oflag=nocache conv=notrunc,fdatasync count=0")
                         .c_str());
    if (res) {
      LOG(WARNING) << "Failed to drop caches.";
      close(fd);
      return -1;
    }
  }

  // Map the file to do major page faults.
  fd = open(filename, O_RDWR);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return -1;
  }
  *buff = reinterpret_cast<uint8_t *>(
      mmap(nullptr, size, PROT_READ | PROT_WRITE,
           prefault ? (MAP_SHARED | MAP_POPULATE) : MAP_SHARED, fd, 0));
  if (*buff == nullptr) {
    LOG(WARNING) << "Failed to map file.";
    return -1;
  }

  return 0;
}

auto BM_SingleEngineMinorPageFault_Compress = [](benchmark::State &state,
                                                 auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto pf_scenario = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);

  auto compressed_buff = reinterpret_cast<uint8_t *>(
      mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Benchmark compress.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;

  // Prepare page faults on source buffer.
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults) {
    if (remmap_memory(&source_buff, mem_size, "compress_src.dat", true, false,
                      false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults) {
    if (remmap_memory(&source_buff, mem_size, "compress_src.dat", false, false,
                      false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    if (remmap_memory(&source_buff, mem_size, "compress_src.dat", false, true,
                      false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    if (single_engine::iaa_translation_fetch(source_buff, mem_size)) {
      LOG(WARNING) << "Failed to prefetch ats translations.";
      for (auto _ : state) {
      }
      goto err;
    }
  }

  // Prepare page faults on dst buffer.
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    memset(compressed_buff, 1, mem_size);
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(compressed_buff, 1, mem_size);
    if (single_engine::iaa_translation_fetch(compressed_buff, mem_size)) {
      LOG(WARNING) << "Failed to prefetch ats translations.";
      for (auto _ : state) {
      }
      goto err;
    }
  }

  // Run benchmark.
  for (auto _ : state) {
    if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                                single_engine::kModeFixed, nullptr,
                                &last_bit_offset, source_buff, mem_size,
                                compressed_buff, &compressed_size)) {
      LOG(WARNING) << "Failed to compress.";
      continue;
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  decompressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
  if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                nullptr, 0, compressed_buff, compressed_size,
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
  munmap(compressed_buff, mem_size);
  free(decompressed_buff);
};

auto BM_SingleEngineMinorPageFault_DeCompress = [](benchmark::State &state,
                                                   auto Inputs...) {
  va_list args;
  va_start(args, Inputs);
  auto pf_scenario = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);

  auto compressed_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));

  //
  uint8_t *decompressed_buff = nullptr;
  size_t decompression_size = 0;

  zero_initialize_counters(state);

  // Compress for verification.
  uint32_t last_bit_offset;
  size_t compressed_size = 0;
  if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                              single_engine::kModeFixed, nullptr,
                              &last_bit_offset, source_buff, mem_size,
                              compressed_buff, &compressed_size)) {
    LOG(WARNING) << "Failed to compress.";
    for (auto _ : state) {
    }
    goto err;
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  decompressed_buff = reinterpret_cast<uint8_t *>(
      mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

  // Prepare page faults on source buffer.
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults) {
    if (remmap_memory(&compressed_buff, compressed_size, "decompress_src.dat",
                      true, false, false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults) {
    if (remmap_memory(&compressed_buff, compressed_size, "decompress_src.dat",
                      false, false, false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    if (remmap_memory(&compressed_buff, compressed_size, "decompress_src.dat",
                      false, true, false)) {
      LOG(WARNING) << "Failed to reload source.";
      for (auto _ : state) {
      }
      goto err;
    }
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    if (single_engine::iaa_translation_fetch(compressed_buff,
                                             compressed_size)) {
      LOG(WARNING) << "Failed to prefetch ats translations.";
      for (auto _ : state) {
      }
      goto err;
    }
  }

  // Prepare page faults on dst buffer.
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    memset(decompressed_buff, 1, mem_size);
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(decompressed_buff, 1, mem_size);
    if (single_engine::iaa_translation_fetch(decompressed_buff, mem_size)) {
      LOG(WARNING) << "Failed to prefetch ats translations.";
      for (auto _ : state) {
      }
      goto err;
    }
  }

  // Benchmark decompress.
  for (auto _ : state) {
    if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                  nullptr, 0, compressed_buff, compressed_size,
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
  // free(compressed_buff);
  munmap(decompressed_buff, mem_size);
};
} // namespace page_faults

#endif
