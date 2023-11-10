#ifndef _QPL_COMPRESS_DECOMPRESS_H_
#define _QPL_COMPRESS_DECOMPRESS_H_

#include <memory>
#include <unistd.h>

#include <glog/logging.h>

#include "qpl/qpl.h"

namespace single_engine {

enum CompressionMode {
  kModeFixed,
  kModeDynamic,
  kModeHuffmanOnly,
  kModeStatic
};

std::unique_ptr<uint8_t[]> init_qpl(qpl_path_t e_path) {
  // Job initialization.
  uint32_t job_size = 0;
  qpl_status status = qpl_get_job_size(e_path, &job_size);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during job size getting.";
    return std::unique_ptr<uint8_t[]>(nullptr);
  }

  std::unique_ptr<uint8_t[]> job_buffer;
  job_buffer = std::make_unique<uint8_t[]>(job_size);
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  status = qpl_init_job(e_path, job);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during compression job initializing.";
    return std::unique_ptr<uint8_t[]>(nullptr);
  }

  return std::move(job_buffer);
}

int free_qpl(qpl_job *job) {
  qpl_status status = qpl_fini_job(job);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during job finalization.";
    return -1;
  }

  return 0;
}

int compress(qpl_path_t e_path, qpl_compression_levels level,
             CompressionMode mode, qpl_huffman_table_t *c_huffman_table,
             uint32_t *last_bit_offset, const uint8_t *src, size_t src_size,
             uint8_t *dst, size_t *dst_size) {
  auto job_buffer = init_qpl(e_path);
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  if (mode == kModeHuffmanOnly) {
    // Create Huffman tables.
    allocator_t default_allocator_c = {malloc, free};
    qpl_status status = qpl_huffman_only_table_create(
        compression_table_type, e_path, default_allocator_c, c_huffman_table);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "Failed to allocate Huffman tables";
      return -1;
    }
  }

  // Compress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_compress;
  job->level = level;
  job->next_in_ptr = const_cast<uint8_t *>(src);
  job->next_out_ptr = dst;
  job->available_in = src_size;
  job->available_out = src_size / 2;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_OMIT_VERIFY | QPL_FLAG_LAST;
  if (mode == kModeDynamic) {
    job->flags |= QPL_FLAG_DYNAMIC_HUFFMAN;
  } else if (mode == kModeHuffmanOnly) {
    job->flags |=
        QPL_FLAG_NO_HDRS | QPL_FLAG_GEN_LITERALS | QPL_FLAG_DYNAMIC_HUFFMAN;
    job->huffman_table = *c_huffman_table;
  } else if (mode == kModeStatic) {
    job->huffman_table = *c_huffman_table;
  } else if (mode == kModeFixed) {
  } else {
    LOG(WARNING) << "Unsupported mode.";
    return -1;
  }

  qpl_status status = qpl_execute_job(job);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error " << status << " acquired during compression.";
    return -1;
  }
  *dst_size = job->total_out;
  *last_bit_offset = job->last_bit_offset;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

int decompress(qpl_path_t e_path, CompressionMode mode,
               qpl_huffman_table_t c_huffman_table, uint32_t last_bit_offset,
               const uint8_t *src, size_t src_size, uint8_t *dst,
               size_t dst_reserved_size, size_t *dst_actual_size) {
  auto job_buffer = init_qpl(e_path);
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  qpl_huffman_table_t d_huffman_table;
  if (mode == kModeHuffmanOnly) {
    // Create Huffman tables.
    allocator_t default_allocator_c = {malloc, free};
    qpl_status status =
        qpl_huffman_only_table_create(decompression_table_type, e_path,
                                      default_allocator_c, &d_huffman_table);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "Failed to allocate Huffman tables";
      return -1;
    }
    // Populate Huffman tables.
    status =
        qpl_huffman_table_init_with_other(d_huffman_table, c_huffman_table);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "Failed to populate Huffman tables";
      return -1;
    }
  }

  // Decompress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_decompress;
  job->next_in_ptr = const_cast<uint8_t *>(src);
  job->next_out_ptr = dst;
  job->available_in = src_size;
  job->available_out = dst_reserved_size;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;
  if (mode == kModeHuffmanOnly) {
    job->flags |= QPL_FLAG_NO_HDRS;
    job->ignore_end_bits = (8 - last_bit_offset) & 7;
    job->huffman_table = d_huffman_table;
  }

  qpl_status status = qpl_execute_job(job);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error " << status << " acquired during decompression.";
    return -1;
  }
  *dst_actual_size = job->total_out;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

} // namespace single_engine

#endif
