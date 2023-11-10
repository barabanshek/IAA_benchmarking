#ifndef _UTIL_H_
#define _UTIL_H_

#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "qpl/qpl.h"

static constexpr uint64_t kkB = 1024;
static constexpr uint64_t kMB = 1024 * 1024;

// Measure time.
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

  if (return_true_entropy) {
    // Compute Shannon entropy.
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
  } else
    return 0;
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

#endif
