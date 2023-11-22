#ifndef _QPL_SCATTERED_H_
#define _QPL_SCATTERED_H_

#include <memory>
#include <unistd.h>

#include <glog/logging.h>

#include "../util.h"

#include "qpl/qpl.h"

namespace single_engine_scattered {

std::unique_ptr<uint8_t[]> init_qpl() {
  // Job initialization.
  uint32_t job_size = 0;
  qpl_status status = qpl_get_job_size(qpl_path_hardware, &job_size);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during job size getting.";
    return std::unique_ptr<uint8_t[]>(nullptr);
  }

  std::unique_ptr<uint8_t[]> job_buffer;
  job_buffer = std::make_unique<uint8_t[]>(job_size);
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  status = qpl_init_job(qpl_path_hardware, job);
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

// static constexpr size_t kPageSize = 4 * 1024;
static constexpr size_t kPageSize = 1 * 64 * 1024;

bool kSplitByConnectedChunks = true;

int compress(const uint8_t *src, size_t src_size, uint8_t *dst,
             size_t *dst_size, std::vector<size_t> &dst_sizes) {
  auto job_buffer = init_qpl();
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // qpl_huffman_table_t huffman_table = nullptr;
  // if (create_static_huffman_tables(qpl_path_hardware, &huffman_table, src,
  //                                  src_size)) {
  //   LOG(WARNING) << "Failed to create huffman tables.";
  //   return -1;
  // }

  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_compress;
  job->level = qpl_default_level;
  job->next_out_ptr = dst;
  job->available_out = src_size - 1;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_OMIT_VERIFY;

  size_t page_size = kPageSize;
  size_t bytes_to_send = src_size;
  size_t it_cnt = 0;
  size_t size_out = 0;
  while (bytes_to_send > 0) {
    job->next_in_ptr = const_cast<uint8_t *>(src) + it_cnt * page_size;
    if (page_size >= bytes_to_send) {
      page_size = bytes_to_send;
      if (kSplitByConnectedChunks)
        job->flags |= QPL_FLAG_LAST;
    }
    if (!kSplitByConnectedChunks)
      job->flags |= QPL_FLAG_LAST;
    job->available_in = page_size;

    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error " << status
                   << " acquired during compression #1.";
      return -1;
    }
    if (kSplitByConnectedChunks)
      size_out = job->total_out - size_out;
    else
      size_out = job->total_out;
    dst_sizes.push_back(size_out);
    bytes_to_send -= page_size;
    ++it_cnt;
    if (kSplitByConnectedChunks)
      job->flags &= ~QPL_FLAG_FIRST;
    if (!kSplitByConnectedChunks)
      *dst_size += job->total_out;
  }

  if (kSplitByConnectedChunks)
    *dst_size = job->total_out;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

int decompress(uint8_t *src, std::vector<size_t> src_sizes, uint8_t *dst,
               size_t dst_reserved_size, size_t *dst_actual_size) {
  auto job_buffer = init_qpl();
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // Decompress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_decompress;
  job->next_out_ptr = dst;
  job->available_out = dst_reserved_size;
  job->flags = QPL_FLAG_OMIT_VERIFY | QPL_FLAG_FIRST;
  size_t offset = 0;
  size_t size_out = 0;
  size_t it_cnt = 0;
  for (auto cur_size : src_sizes) {
    job->next_in_ptr = const_cast<uint8_t *>(src) + offset;
    job->available_in = cur_size;
    job->next_out_ptr = dst + size_out;

    if (it_cnt == src_sizes.size() - 1) {
      if (kSplitByConnectedChunks)
        job->flags |= QPL_FLAG_LAST;
    }
    if (!kSplitByConnectedChunks)
      job->flags |= QPL_FLAG_LAST;
    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error " << status
                   << " acquired during decompression.";
      return -1;
    }

    if (kSplitByConnectedChunks)
      size_out = job->total_out;
    else
      size_out += job->total_out;
    // assert(size_out == 256 * 1024 * 1024);
    offset += cur_size;
    if (kSplitByConnectedChunks)
      job->flags &= ~QPL_FLAG_FIRST;
    if (!kSplitByConnectedChunks)
      *dst_actual_size = size_out;
  }

  if (kSplitByConnectedChunks)
    *dst_actual_size = job->total_out;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

} // namespace single_engine_scattered

#endif
