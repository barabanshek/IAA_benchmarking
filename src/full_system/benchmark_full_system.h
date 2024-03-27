#ifndef _BENCHMARK_FULL_SYSTEM_H_
#define _BENCHMARK_FULL_SYSTEM_H_

#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include <benchmark/benchmark.h>

#include "../single_engine/qpl_compress_decompress.h"
#include "../util.h"

namespace full_system {

static int prepare_compressed_files(uint8_t *src, size_t src_size,
                                    const char *filename,
                                    size_t *compressed_size_out) {
  // Compress.
  size_t compressed_size = 2 * src_size;
  auto compressed_buff = malloc_allocate(compressed_size);
  memset(compressed_buff.get(), _PAGE_PREFAULT_, compressed_size);
  if (single_engine::compress(qpl_path_hardware, qpl_default_level,
                              single_engine::kModeDynamic, nullptr, nullptr,
                              src, src_size, compressed_buff.get(),
                              &compressed_size)) {
    LOG(WARNING) << "Failed to compress memory.";
    return -1;
  }

  // Align with page size to allow smoth O_DIRECT.
  size_t compress_size_aligned = (compressed_size + 4095) & (~4095);

  // Write to file.
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0x666);
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file.";
    return -1;
  }

  ssize_t s = write(fd, compressed_buff.get(), compress_size_aligned);
  if (s == -1 || static_cast<size_t>(s) != compress_size_aligned) {
    LOG(WARNING) << "Failed to write pf file.";
    return -1;
  }
  fsync(fd);
  close(fd);

  *compressed_size_out = compressed_size;
  return 0;
}

enum FullSystemMode {
  kBenchmarkDiskRead,
  kBenchmarkDiskReadIODirect,
  kBenchmarkDecompress,
  kBenchmarkDecompressFromFile,
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
  auto compressed_size = va_arg(args, size_t);
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
  int fd = -1;
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDiskReadIODirect) {
    fd = open(filename, O_RDWR | O_DIRECT);
  } else {
    fd = open(filename, O_RDWR);
  }
  if (fd == -1) {
    LOG(WARNING) << "Failed to open file: " << filename;
    return -1;
  }
  size_t mem_size = static_cast<size_t>(lseek(fd, 0L, SEEK_END));
  state.counters["File Size"] = mem_size;
  lseek(fd, 0L, SEEK_SET);

  if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompressFromFile) {
    if (posix_fadvise(fd, 0x00, mem_size, POSIX_FADV_SEQUENTIAL)) {
      state.SkipWithMessage("posix_fadvise failed");
    }
  }

  // Allocate memory.
  uint8_t *mem_buff = nullptr;
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDiskRead ||
      static_cast<FullSystemMode>(mode) == kBenchmarkDiskReadIODirect) {
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
      state.SkipWithMessage(
          std::string(
              "Failed to read file for kBenchmarkDecompress, error was: ") +
          std::strerror(errno));
      goto err;
    }
  }
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDecompressFromFile) {
    // Mmap file.
    mem_buff = reinterpret_cast<uint8_t *>(
        mmap(nullptr, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    // mem_buff = reinterpret_cast<uint8_t *>(
    //     mmap(nullptr, mem_size, PROT_READ | PROT_WRITE,
    //          MAP_SHARED | MAP_ANONYMOUS , -1, 0));
  }

  // Benchmark.
  if (static_cast<FullSystemMode>(mode) == kBenchmarkDiskRead ||
      static_cast<FullSystemMode>(mode) == kBenchmarkDiskReadIODirect) {
    // Benchmark read file.
    for (auto _ : state) {
      ssize_t res = read(fd, mem_buff, mem_size);
      if (res == -1 || static_cast<size_t>(res) != mem_size) {
        state.SkipWithMessage(
            std::string("Failed to read file for kBenchmarkDiskRead") +
            std::strerror(errno) + ", size= " + std::to_string(mem_size));
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
        // usleep(10000);
        // t1.join();

        // std::cout << "Read done: " << *(uint64_t*)mem_buff << std::endl;

        // int res = -1;
        // while (res) {
        //   res = single_engine::decompress(
        //       qpl_path_hardware, single_engine::kModeDynamic, nullptr, 0,
        //       mem_buff, compressed_size, decompressed_buff.get(),
        //       decompression_expected_size, &decompression_size);
        // }

        // usleep(100);

        if (single_engine::decompress(
                qpl_path_hardware, single_engine::kModeDynamic, nullptr, 0,
                mem_buff, compressed_size, decompressed_buff.get(),
                decompression_expected_size, &decompression_size)) {
          state.SkipWithMessage("Failed to decompress.");
          goto err;
        }

        // for (size_t i=0; i<compressed_size; i += 4096) {
        //   volatile uint8_t* data = mem_buff + i;
        //   *data;
        // }

        // t1.join();
      } else {
        if (single_engine::decompress(
                qpl_path_hardware, single_engine::kModeDynamic, nullptr, 0,
                mem_buff, compressed_size, decompressed_buff.get(),
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

} // namespace full_system

#endif
