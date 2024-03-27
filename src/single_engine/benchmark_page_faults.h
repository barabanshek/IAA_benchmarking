#ifndef _BENCHMARK_PAGE_FAULTS_H_
#define _BENCHMARK_PAGE_FAULTS_H_

#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_compress_decompress.h"

namespace page_faults {

enum PageFaultScenario {
  kMajorPageFaults,
  kMinorPageFaults,
  kAtsMiss,
  kNoFaults
};

#define _PARSE_ARGS__                                                          \
  _PARSE_IN                                                                    \
  auto pf_scenario = Inputs;                                                   \
  auto source_size = _PARSE_ARG(size_t);                                       \
  auto source_buff = _PARSE_ARG(uint8_t *);                                    \
  _PARSE_OUT

/// Re-mmap memory @param buff from a file to allow major page faults later (if
/// @param prefault = false).
static std::unique_ptr<uint8_t, MMapDeleter>
remmap_memory_through_file(uint8_t *buff, size_t size, const char *filename,
                           bool drop_cache, bool prefault) {
  // Dump source buffer to file for major page faults.
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0x666);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return std::unique_ptr<uint8_t, MMapDeleter>(nullptr);
  }
  ssize_t s = write(fd, buff, size);
  if (s == -1 || static_cast<size_t>(s) != size) {
    LOG(WARNING) << "Failed to write pf file.";
    return std::unique_ptr<uint8_t, MMapDeleter>(nullptr);
  }
  fsync(fd);
  close(fd);

  // Drop caches.
  if (drop_cache) {
    if (system((std::string("sudo dd of=") + filename +
                " oflag=nocache conv=notrunc,fdatasync count=0")
                   .c_str())) {
      LOG(WARNING) << "Failed to drop caches.";
      return std::unique_ptr<uint8_t, MMapDeleter>(nullptr);
    }
  }

  // Map the file to do major page faults.
  fd = open(filename, O_RDWR);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return std::unique_ptr<uint8_t, MMapDeleter>(nullptr);
  }
  uint8_t *ptr = reinterpret_cast<uint8_t *>(
      mmap(nullptr, size, PROT_READ | PROT_WRITE,
           prefault ? (MAP_SHARED | MAP_POPULATE) : MAP_SHARED, fd, 0));
  if (ptr == nullptr) {
    LOG(WARNING) << "Failed to map file.";
    return std::unique_ptr<uint8_t, MMapDeleter>(nullptr);
  }

  std::unique_ptr<uint8_t, MMapDeleter> unique_ptr(ptr);
  unique_ptr.get_deleter().set_size(size);
  return unique_ptr;
}

auto BM_SingleEngineMinorPageFault_Compress = [](benchmark::State &state,
                                                 auto Inputs...) {
  // Parse input.
  _PARSE_ARGS__
  assert(source_buff != nullptr);

  // Set default counters.
  zero_initialize_counters(state);

  // Prepare page faults on source buffer.
  std::unique_ptr<uint8_t, MMapDeleter> new_source_buff;
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults)
    new_source_buff = remmap_memory_through_file(
        source_buff, source_size, "compress_src.dat", true, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults)
    new_source_buff = remmap_memory_through_file(
        source_buff, source_size, "compress_src.dat", false, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    new_source_buff = remmap_memory_through_file(
        source_buff, source_size, "compress_src.dat", false, true);

  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    new_source_buff = remmap_memory_through_file(
        source_buff, source_size, "compress_src.dat", false, true);
    if (single_engine::iaa_translation_fetch(new_source_buff.get(),
                                             source_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  if (new_source_buff.get() == nullptr)
    state.SkipWithMessage("Failed to remmap source.");

  // Prepare page faults on dst buffer.
  size_t compressed_size = 2 * source_size;
  auto compressed_buff = mmap_allocate(compressed_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
    if (single_engine::iaa_translation_fetch(compressed_buff.get(),
                                             compressed_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  // Run benchmark.
  for (auto _ : state) {
    if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                                single_engine::kModeFixed, nullptr, nullptr,
                                new_source_buff.get(), source_size,
                                compressed_buff.get(), &compressed_size)) {
      state.SkipWithMessage("Failed to compress.");
    }
  }
  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(source_size);
  size_t decompression_size = 0;
  if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                nullptr, 0, compressed_buff.get(),
                                compressed_size, decompressed_buff.get(),
                                source_size, &decompression_size))
    state.SkipWithMessage("Failed to decompress.");
  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
  return 0;
};

auto BM_SingleEngineMinorPageFault_DeCompress = [](benchmark::State &state,
                                                   auto Inputs...) {
  // Parse input.
  _PARSE_ARGS__
  assert(source_buff != nullptr);

  // Set default counters.
  zero_initialize_counters(state);

  // Compress for verification.
  size_t compressed_size = 2 * source_size;
  auto compressed_buff = mmap_allocate(compressed_size);
  if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                              single_engine::kModeFixed, nullptr, nullptr,
                              source_buff, source_size, compressed_buff.get(),
                              &compressed_size))
    state.SkipWithMessage("Failed to compress.");
  state.counters["Compression Ratio"] = 1.0 * source_size / compressed_size;

  // Prepare page faults on source buffer.
  std::unique_ptr<uint8_t, MMapDeleter> new_compressed_buff;
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults)
    new_compressed_buff =
        remmap_memory_through_file(compressed_buff.get(), compressed_size,
                                   "decompress_src.dat", true, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults)
    new_compressed_buff =
        remmap_memory_through_file(compressed_buff.get(), compressed_size,
                                   "decompress_src.dat", false, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    new_compressed_buff =
        remmap_memory_through_file(compressed_buff.get(), compressed_size,
                                   "decompress_src.dat", false, true);

  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    new_compressed_buff =
        remmap_memory_through_file(compressed_buff.get(), compressed_size,
                                   "decompress_src.dat", false, true);
    if (single_engine::iaa_translation_fetch(new_compressed_buff.get(),
                                             compressed_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  if (new_compressed_buff.get() == nullptr)
    state.SkipWithMessage("Failed to remmap source.");

  // Prepare page faults on dst buffer.
  auto decompressed_buff = mmap_allocate(source_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    memset(decompressed_buff.get(), _PAGE_PREFAULT_, source_size);
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(decompressed_buff.get(), _PAGE_PREFAULT_, source_size);
    if (single_engine::iaa_translation_fetch(decompressed_buff.get(),
                                             source_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                  nullptr, 0, new_compressed_buff.get(),
                                  compressed_size, decompressed_buff.get(),
                                  source_size, &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != source_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
  return 0;
};

} // namespace page_faults

#endif
