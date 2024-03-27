#ifndef _QPL_PARALLEL_H_
#define _QPL_PARALLEL_H_

#include <memory>
#include <unistd.h>
#include <vector>

#include <glog/logging.h>

#include "../util.h"

#include "qpl/qpl.h"

namespace multi_engine {

enum CompressionMode { kParallelFixed, kParallelDynamic, kParallelCanned };

// [<compressed_data, original_size>].
typedef std::vector<std::tuple<std::vector<uint8_t>, size_t>> CompressedFormat;
typedef std::vector<std::unique_ptr<uint8_t[]>> MultiChunkJob;

MultiChunkJob init_qpl(qpl_path_t e_path, uint8_t threads) {
  // Job initialization.
  uint32_t job_size = 0;
  qpl_status status = qpl_get_job_size(e_path, &job_size);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during job size getting.";
    return MultiChunkJob();
  }

  MultiChunkJob job_buffers;
  for (uint8_t i = 0; i < threads; ++i) {
    std::unique_ptr<uint8_t[]> job_buffer;
    job_buffer = std::make_unique<uint8_t[]>(job_size);
    auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
    status = qpl_init_job(e_path, job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error acquired during compression job initializing.";
      return MultiChunkJob();
    }

    job_buffers.push_back(std::move(job_buffer));
  }

  return job_buffers;
}

int free_qpl(MultiChunkJob &job_buffers) {
  for (auto &job_buffer : job_buffers) {
    auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
    qpl_status status = qpl_fini_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error acquired during job finalization.";
      return -1;
    }
  }

  return 0;
}

int compress(CompressionMode mode, const uint8_t *src, size_t src_size,
             CompressedFormat *compressed_buff) {
  size_t thread_count = compressed_buff->size();
  auto job_buffers = init_qpl(qpl_path_hardware, thread_count);
  if (job_buffers.empty()) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  qpl_huffman_table_t huffman_table = nullptr;
  if (mode == kParallelCanned) {
    if (create_static_huffman_tables(qpl_path_hardware, &huffman_table, src,
                                     src_size)) {
      LOG(WARNING) << "Failed to create huffman tables.";
      return -1;
    }
  }

  // Submit compress.
  size_t src_offst = 0;
  size_t chunk_cnt = 0;
  for (auto &job_buffer : job_buffers) {
    size_t in_chunk_size = std::get<1>(compressed_buff->at(chunk_cnt));
    size_t available_chunk_size =
        std::get<0>(compressed_buff->at(chunk_cnt)).size();

    auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
    job->op = qpl_op_compress;
    job->level = qpl_default_level;
    job->next_in_ptr = const_cast<uint8_t *>(src) + src_offst;
    job->available_in = in_chunk_size;
    job->next_out_ptr = std::get<0>(compressed_buff->at(chunk_cnt)).data();
    job->available_out = available_chunk_size;
    job->flags = QPL_FLAG_FIRST | QPL_FLAG_OMIT_VERIFY | QPL_FLAG_LAST;

    if (mode == kParallelCanned) {
      job->huffman_table = huffman_table;
    } else if (mode == kParallelDynamic) {
      job->flags |= QPL_FLAG_DYNAMIC_HUFFMAN;
    }

    qpl_status status = qpl_submit_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error " << status
                   << " acquired during compression job submission.";
      return -1;
    }

    src_offst += in_chunk_size;
    ++chunk_cnt;
  }

  // Wait for compression and gather.
  std::vector<uint8_t> cmpl(thread_count, 0);
  while (std::reduce(cmpl.begin(), cmpl.end()) != thread_count) {
    for (size_t i = 0; i < job_buffers.size(); ++i) {
      if (cmpl[i] == 0) {
        qpl_job *job = reinterpret_cast<qpl_job *>(job_buffers[i].get());
        auto status = qpl_check_job(job);
        if (status != QPL_STS_BEING_PROCESSED) {
          if (status != QPL_STS_OK) {
            LOG(WARNING) << "An error " << status
                         << " acquired during awaiting for completion";
            return -1;
          }
          std::get<0>(compressed_buff->at(i)).resize(job->total_out);
          cmpl[i] = 1;
        }
      }
    }
  }

  if (free_qpl(job_buffers)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

int decompress(CompressedFormat &compressed_buff, uint8_t *dst,
               size_t *dst_actual_size) {
  size_t thread_count = compressed_buff.size();
  auto job_buffers = init_qpl(qpl_path_hardware, thread_count);
  if (job_buffers.empty()) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // Submit decompress.
  size_t dst_offst = 0;
  size_t chunk_cnt = 0;
  for (auto &job_buffer : job_buffers) {
    size_t decompress_chunk_size = std::get<1>(compressed_buff[chunk_cnt]);

    auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
    job->op = qpl_op_decompress;
    job->next_in_ptr = std::get<0>(compressed_buff[chunk_cnt]).data();
    job->available_in = std::get<0>(compressed_buff[chunk_cnt]).size();
    job->next_out_ptr = const_cast<uint8_t *>(dst) + dst_offst;
    job->available_out = decompress_chunk_size;
    job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;

    qpl_status status = qpl_submit_job(job);
    if (status != QPL_STS_OK) {
      LOG(WARNING) << "An error " << status
                   << " acquired during compression job submission.";
      return -1;
    }

    dst_offst += decompress_chunk_size;
    ++chunk_cnt;
  }

  // Wait for decompression.
  size_t decompress_size = 0;
  std::vector<uint8_t> cmpl(thread_count, 0);
  while (std::reduce(cmpl.begin(), cmpl.end()) != thread_count) {
    for (size_t i = 0; i < job_buffers.size(); ++i) {
      if (cmpl[i] == 0) {
        qpl_job *job = reinterpret_cast<qpl_job *>(job_buffers[i].get());
        auto status = qpl_check_job(job);
        if (status != QPL_STS_BEING_PROCESSED) {
          if (status != QPL_STS_OK) {
            LOG(WARNING) << "An error " << status
                         << " acquired during awaiting for completion";
            return -1;
          }
          decompress_size += job->total_out;
          cmpl[i] = 1;
        }
      }
    }
  }

  *dst_actual_size = decompress_size;
  return 0;
}

} // namespace multi_engine

#endif
