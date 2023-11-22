#ifndef _QPL_CANNED_H_
#define _QPL_CANNED_H_

#include <memory>
#include <unistd.h>

#include <glog/logging.h>

#include "../util.h"

#include "qpl/qpl.h"

namespace single_engine_canned {

enum CompressionMode { kContinious, kNaive, kCanned };

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

int compress(CompressionMode mode, const uint8_t *src, size_t src_size,
             uint8_t *dst, size_t *dst_size, size_t chunk_size,
             qpl_huffman_table_t *huffman_table) {
  auto job_buffer = init_qpl(qpl_path_hardware);
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  if (mode == kCanned) {
    if (create_static_huffman_tables(qpl_path_hardware, huffman_table, src,
                                     src_size)) {
      LOG(WARNING) << "Failed to create huffman tables.";
      return -1;
    }
  }

  // Compress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_compress;
  job->level = qpl_default_level;
  job->next_out_ptr = dst;
  job->available_out = src_size - 1;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_OMIT_VERIFY;

  if (mode == kContinious) {
    job->next_in_ptr = const_cast<uint8_t *>(src);
    job->available_in = src_size;
    job->flags |= QPL_FLAG_DYNAMIC_HUFFMAN | QPL_FLAG_LAST;
    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error " << status << " acquired during compression.";
      return -1;
    }
  } else {
    if (mode == kNaive) {
      job->flags |= QPL_FLAG_DYNAMIC_HUFFMAN;
      job->huffman_table = nullptr;
    } else if (mode == kCanned) {
      // job->flags |= QPL_FLAG_CANNED_MODE;
      job->huffman_table = *huffman_table;
    }

    // Perform scattered chunk compression.
    size_t src_bytes_left = src_size;
    size_t it_cnt = 0;
    while (src_bytes_left > 0) {
      job->next_in_ptr = const_cast<uint8_t *>(src) + it_cnt * chunk_size;

      if (chunk_size >= src_bytes_left) {
        job->flags |= QPL_FLAG_LAST;
        chunk_size = src_bytes_left;
      }

      src_bytes_left -= chunk_size;
      job->available_in = chunk_size;

      qpl_status status = qpl_execute_job(job);
      if (status != QPL_STS_OK) {
        LOG(WARNING) << "An error " << status
                     << " acquired during compression.";
        return -1;
      }

      job->flags &= ~QPL_FLAG_FIRST;
      ++it_cnt;
    }
  }

  *dst_size = job->total_out;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

int decompress(uint8_t *src, size_t src_size, uint8_t *dst,
               size_t dst_reserved_size, size_t *dst_actual_size,
               qpl_huffman_table_t huffman_table) {
  auto job_buffer = init_qpl(qpl_path_hardware);
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // Decompress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_decompress;
  job->next_in_ptr = src;
  job->available_in = src_size;
  job->next_out_ptr = dst;
  job->available_out = dst_reserved_size;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST; // | QPL_FLAG_CANNED_MODE;
  job->huffman_table = huffman_table;

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

} // namespace single_engine_canned

#endif
