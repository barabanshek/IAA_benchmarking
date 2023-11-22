#ifndef _BENCHMARK_PAGE_FAULTS_H_
#define _BENCHMARK_PAGE_FAULTS_H_

#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include "../util.h"
#include "qpl_canned.h"
#include "qpl_compress_decompress.h"

namespace page_faults {

enum PageFaultScenario {
  kMajorPageFaults,
  kMinorPageFaults,
  kAtsMiss,
  kNoFaults
};

enum FullSystemMode {
  kBenchmarkDiskRead,
  kBenchmarkDecompress,
  kBenchmarkDecompressFromFile,
};

static int prepare_compressed_files(uint8_t *src, size_t src_size,
                                    const char *filename) {
  // Compress.
  auto compressed_buff = malloc_allocate(src_size);
  memset(compressed_buff.get(), 1, src_size);
  size_t compressed_size = 0;
  if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                              single_engine::kModeDynamic, nullptr, nullptr,
                              src, src_size, compressed_buff.get(),
                              &compressed_size)) {
    LOG(WARNING) << "Failed to compress memory.";
    return -1;
  }

  // Write to file.
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0x666);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return -1;
  }

  ssize_t s = write(fd, compressed_buff.get(), compressed_size);
  if (s == -1 || static_cast<size_t>(s) != compressed_size) {
    LOG(WARNING) << "Failed to write pf file.";
    return -1;
  }
  fsync(fd);
  close(fd);

  return 0;
}

auto BM_SingleEngineMinorPageFault_Compress = [](benchmark::State &state,
                                                 auto Inputs...) {
  // Parse input.
  va_list args;
  va_start(args, Inputs);
  auto pf_scenario = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  // Set default counters.
  zero_initialize_counters(state);

  // Prepare page faults on source buffer.
  std::unique_ptr<uint8_t, MMapDeleter> new_source_buff;
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults)
    new_source_buff = remmap_memory_through_file(
        source_buff, mem_size, "compress_src.dat", true, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults)
    new_source_buff = remmap_memory_through_file(
        source_buff, mem_size, "compress_src.dat", false, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    new_source_buff = remmap_memory_through_file(
        source_buff, mem_size, "compress_src.dat", false, true);

  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    new_source_buff = remmap_memory_through_file(
        source_buff, mem_size, "compress_src.dat", false, true);
    if (single_engine::iaa_translation_fetch(new_source_buff.get(), mem_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  if (new_source_buff.get() == nullptr)
    state.SkipWithMessage("Failed to remmap source.");

  // Prepare page faults on dst buffer.
  auto compressed_buff = mmap_allocate(mem_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    memset(compressed_buff.get(), 1, mem_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(compressed_buff.get(), 1, mem_size);
    if (single_engine::iaa_translation_fetch(compressed_buff.get(), mem_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  // Run benchmark.
  size_t compressed_size;
  for (auto _ : state) {
    if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                                single_engine::kModeFixed, nullptr, nullptr,
                                new_source_buff.get(), mem_size,
                                compressed_buff.get(), &compressed_size)) {
      state.SkipWithMessage("Failed to compress.");
    }
  }
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Verify with decompress.
  auto decompressed_buff = malloc_allocate(mem_size);
  size_t decompression_size = 0;
  if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                nullptr, 0, compressed_buff.get(),
                                compressed_size, decompressed_buff.get(),
                                mem_size, &decompression_size))
    state.SkipWithMessage("Failed to decompress.");
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
  return 0;
};

auto BM_SingleEngineMinorPageFault_DeCompress = [](benchmark::State &state,
                                                   auto Inputs...) {
  // Parse input.
  va_list args;
  va_start(args, Inputs);
  auto pf_scenario = Inputs;
  size_t mem_size = va_arg(args, size_t);
  uint8_t *source_buff = va_arg(args, uint8_t *);
  assert(source_buff != nullptr);
  va_end(args);

  // Set default counters.
  zero_initialize_counters(state);

  // Compress for verification.
  auto compressed_buff = mmap_allocate(mem_size);
  size_t compressed_size = 0;
  if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                              single_engine::kModeFixed, nullptr, nullptr,
                              source_buff, mem_size, compressed_buff.get(),
                              &compressed_size))
    state.SkipWithMessage("Failed to compress.");
  state.counters["Compression Ratio"] = 1.0 * mem_size / compressed_size;

  // Prepare page faults on source buffer.
  std::unique_ptr<uint8_t, MMapDeleter> new_compressed_buff;
  if (static_cast<PageFaultScenario>(pf_scenario) == kMajorPageFaults)
    new_compressed_buff = remmap_memory_through_file(
        compressed_buff.get(), mem_size, "decompress_src.dat", true, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kMinorPageFaults)
    new_compressed_buff = remmap_memory_through_file(
        compressed_buff.get(), mem_size, "decompress_src.dat", false, false);

  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss)
    new_compressed_buff = remmap_memory_through_file(
        compressed_buff.get(), mem_size, "decompress_src.dat", false, true);

  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    new_compressed_buff = remmap_memory_through_file(
        compressed_buff.get(), mem_size, "decompress_src.dat", false, true);
    if (single_engine::iaa_translation_fetch(new_compressed_buff.get(),
                                             mem_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  if (new_compressed_buff.get() == nullptr)
    state.SkipWithMessage("Failed to remmap source.");

  // Prepare page faults on dst buffer.
  auto decompressed_buff = mmap_allocate(mem_size);
  if (static_cast<PageFaultScenario>(pf_scenario) == kAtsMiss) {
    memset(decompressed_buff.get(), 1, mem_size);
  }
  if (static_cast<PageFaultScenario>(pf_scenario) == kNoFaults) {
    memset(decompressed_buff.get(), 1, mem_size);
    if (single_engine::iaa_translation_fetch(decompressed_buff.get(), mem_size))
      state.SkipWithMessage("Failed to prefetch ats translations.");
  }

  // Benchmark decompress.
  size_t decompression_size = 0;
  for (auto _ : state) {
    if (single_engine::decompress(qpl_path_hardware, single_engine::kModeFixed,
                                  nullptr, 0, new_compressed_buff.get(),
                                  compressed_size, decompressed_buff.get(),
                                  mem_size, &decompression_size))
      state.SkipWithMessage("Failed to decompress.");
  }

  // Verify.
  if (decompression_size != mem_size ||
      memcmp(source_buff, decompressed_buff.get(), decompression_size) != 0)
    state.SkipWithMessage("Data missmatch.");

  state.counters["Status"] = 0;
  return 0;
};

struct Arg {
  int fd;
  uint8_t *mem_buff;
  size_t mem_size;
};
void task(void *arg) {
  auto arg_ = reinterpret_cast<Arg *>(arg);
  read(arg_->fd, arg_->mem_buff, arg_->mem_size);
}

auto BM_FullSystem = [](benchmark::State &state, auto Inputs...) {
  // Parse input.
  va_list args;
  va_start(args, Inputs);
  auto mode = Inputs;
  auto decompression_expected_size = va_arg(args, size_t);
  auto filename = va_arg(args, char *);
  va_end(args);

  // Set default counters.
  zero_initialize_counters(state);

  // Drop page cache.
  if (system((std::string("sudo dd of=") + filename +
              " oflag=nocache conv=notrunc,fdatasync count=0")
                 .c_str())) {
    LOG(WARNING) << "Failed to drop caches.";
    return -1;
  }

  // Open file.
  int fd = open(filename, O_RDWR);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return -1;
  }
  size_t mem_size = static_cast<size_t>(lseek(fd, 0L, SEEK_END));
  state.counters["File Size"] = mem_size;
  lseek(fd, 0L, SEEK_SET);

  // Allocate memory.
  uint8_t *mem_buff = nullptr;
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDiskRead) {
    // Just mmap.
    mem_buff = reinterpret_cast<uint8_t *>(
        mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
  }
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompress) {
    // Mmap and read.
    mem_buff = reinterpret_cast<uint8_t *>(
        mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    ssize_t res = read(fd, mem_buff, mem_size);
    if (res == -1 || static_cast<size_t>(res) != mem_size) {
      state.SkipWithMessage("Failed to read file");
      goto err;
    }
  }
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompressFromFile) {
    // Mmap file.
    mem_buff = reinterpret_cast<uint8_t *>(
        mmap(nullptr, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    // mem_buff = reinterpret_cast<uint8_t *>(
    //     mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
    //          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  }

  // Benchmark.
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDiskRead) {
    // Benchmark read file.
    for (auto _ : state) {
      ssize_t res = read(fd, mem_buff, mem_size);
      if (res == -1 || static_cast<size_t>(res) != mem_size) {
        state.SkipWithMessage("Failed to read file.");
        goto err;
      }
    }
  }
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompress ||
      static_cast<FullSystemMode>(mode) == kBenchmarkDecompressFromFile) {
    auto decompressed_buff = malloc_allocate(decompression_expected_size);
    memset(decompressed_buff.get(), 1, decompression_expected_size);
    size_t decompression_size = 0;
    for (auto _ : state) {
      // read(fd, mem_buff, mem_size);
      if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompressFromFile) {
        // Arg args = {.fd = fd, .mem_buff = mem_buff, .mem_size = mem_size};
        // std::thread t1(task, &args);
        // usleep(100000);
        // t1.join();

        if (single_engine::decompress(
                qpl_path_hardware, single_engine::kModeDynamic, nullptr, 0,
                mem_buff, mem_size, decompressed_buff.get(),
                decompression_expected_size, &decompression_size)) {
          state.SkipWithMessage("Failed to decompress.");
          goto err;
        }

        // t1.join();
      } else {
        if (single_engine::decompress(
                qpl_path_hardware, single_engine::kModeDynamic, nullptr, 0,
                mem_buff, mem_size, decompressed_buff.get(),
                decompression_expected_size, &decompression_size)) {
          state.SkipWithMessage("Failed to decompress.");
          goto err;
        }
      }
    }

    if (decompression_size != decompression_expected_size) {
      state.SkipWithMessage("Data missmatch.");
    }
  }

err:
  munmap(mem_buff, mem_size);
  close(fd);
  return 0;
};

} // namespace page_faults

#endif
