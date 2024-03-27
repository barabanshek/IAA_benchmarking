#ifndef _UTIL_H_
#define _UTIL_H_

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>

#include "qpl/qpl.h"
#include <benchmark/benchmark.h>

// Common defs for benchmarks.
#define _PARSE_IN                                                              \
  va_list args;                                                                \
  va_start(args, Inputs);
#define _PARSE_OUT va_end(args);
#define _PARSE_ARG(X) va_arg(args, X)

// Fill page with '1's to prefault it; 0-initialization does not work due to the
// zero-page optimization.
#define _PAGE_PREFAULT_ 1

// Some nice constants.
static constexpr uint64_t kkB = 1024;
static constexpr uint64_t kMB = 1024 * 1024;

//
/// Measure time.
//
class TimeScope {
public:
  TimeScope() { start_tick = std::chrono::high_resolution_clock::now(); }

  template <class T> auto GetTimeStamp() {
    auto now_tick = std::chrono::high_resolution_clock::now();
    auto delta_tick = std::chrono::duration_cast<T>(now_tick - start_tick);
    return delta_tick.count();
  }

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_tick;
};

//
/// Memory allocators.
//
class MMapDeleter {
public:
  void operator()(void *ptr) const {
    if (ptr != nullptr)
      munmap(ptr, size_);
  }

  void set_size(size_t size) { size_ = size; }

private:
  size_t size_ = 0;
};

class MallocDeleter {
public:
  void operator()(void *ptr) const {
    if (ptr != nullptr)
      free(ptr);
  }
};

std::unique_ptr<uint8_t, MMapDeleter> mmap_allocate(size_t size) {
  uint8_t *ptr =
      reinterpret_cast<uint8_t *>(mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  std::unique_ptr<uint8_t, MMapDeleter> unique_ptr(ptr);
  unique_ptr.get_deleter().set_size(size);
  return unique_ptr;
}

std::unique_ptr<uint8_t, MallocDeleter> malloc_allocate(size_t size) {
  return std::unique_ptr<uint8_t, MallocDeleter>(
      reinterpret_cast<uint8_t *>(malloc(size)));
}

// Initialize all supported counters with zero.
void zero_initialize_counters(benchmark::State &state) {
  state.counters["Compression Time"] = 0;
  state.counters["Compression Ratio"] = 0;
  state.counters["File Size"] = 0;
  state.counters["Status"] = -1;
}

/// Compute Shannon entropy.
double compute_entropy(uint8_t *mem, size_t size) {
  constexpr uint16_t kMaxBytes = 256;
  std::vector<uint8_t> probablilities(kMaxBytes);
  std::fill(probablilities.begin(), probablilities.end(), 0);
  for (size_t i = 0; i < size; ++i) {
    uint8_t byte = *(mem + i);
    ++probablilities[byte];
  }
  double entropy = 0.0;
  for (uint16_t i = 0; i < kMaxBytes; ++i) {
    double temp = static_cast<double>(probablilities[i]) / size;
    if (temp > 0.)
      entropy += temp * std::fabs(std::log2(temp));
  }

  return entropy;
}

/// @param entropy -- when bigger, the entrpy is bigger.
/// Returns the true Shannon entropy.
double init_rand_memory(uint8_t *mem, size_t size, uint16_t entropy,
                        bool return_true_entropy = true) {
  std::random_device rd{};
  std::mt19937 gen{rd()};
  std::normal_distribution generator;
  std::uniform_int_distribution<> generator_uniform(1, entropy);

  if (entropy == 0)
    generator = std::normal_distribution(0.0, 0.0);
  else
    generator = std::normal_distribution(0.0, (1 << entropy) * 1.0);

  // Generate.
  constexpr uint16_t kZeroRegionSize = 512;
  size_t i = 0;
  while (i < size) {
    if (i < size - kZeroRegionSize && generator_uniform(gen) == 1) {
      // Insert zero block.
      memset(mem + i, 0, kZeroRegionSize);
      i += kZeroRegionSize;
    } else {
      *reinterpret_cast<uint32_t *>(mem + i) = std::round(generator(gen));
      i += sizeof(uint32_t);
    }
  }

  if (return_true_entropy)
    return compute_entropy(mem, size);
  else
    return 0;
}

typedef std::vector<std::tuple<size_t, std::string, double, uint8_t *>>
    CompressionDataset;
CompressionDataset load_corpus_dataset(const char *dataset_path) {
  assert(std::filesystem::exists(dataset_path) &&
         std::filesystem::is_directory(dataset_path));
  LOG(INFO) << "Loading dataset from " << dataset_path;
  CompressionDataset dataset;
  for (const auto &entry : std::filesystem::directory_iterator(dataset_path)) {
    std::string filename = entry.path().filename();
    filename = std::string(dataset_path) + "/" + filename;
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
      LOG(FATAL) << "failed to open benchmark file " << filename << ", "
                 << strerror(errno);
    ssize_t fd_size = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);
    LOG(INFO) << "Found file: " << filename << " of size: " << fd_size << " B";
    uint8_t *mem =
        reinterpret_cast<uint8_t *>(malloc(static_cast<size_t>(fd_size)));
    if (read(fd, mem, static_cast<size_t>(fd_size)) != fd_size)
      LOG(FATAL) << "Failed to read benchmark file " << filename;
    double entropy = compute_entropy(mem, static_cast<size_t>(fd_size));
    dataset.push_back(std::make_tuple(fd_size, filename, entropy, mem));
  }

  LOG(INFO) << "Dataset with " << dataset.size() << " files is loaded";
  return dataset;
}

int create_static_huffman_tables(qpl_path_t e_path,
                                 qpl_huffman_table_t *c_huffman_table,
                                 const uint8_t *src, size_t src_size) {
  // Create Huffman tables.
  qpl_status status = qpl_deflate_huffman_table_create(
      combined_table_type, e_path, DEFAULT_ALLOCATOR_C, c_huffman_table);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "Failed to allocate Huffman tables";
    return -1;
  }

  // Gather statistics.
  qpl_histogram histogram{};
  status = qpl_gather_deflate_statistics(const_cast<uint8_t *>(src), src_size,
                                         &histogram, qpl_default_level, e_path);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "Failed to gather statistics.";
    qpl_huffman_table_destroy(*c_huffman_table);
    return -1;
  }

  // Populate the Huffman tabes with the statistics.
  status = qpl_huffman_table_init_with_histogram(*c_huffman_table, &histogram);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "Failed to populate the Huffman tabels.";
    qpl_huffman_table_destroy(*c_huffman_table);
    return -1;
  }

  return 0;
}

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

#endif
