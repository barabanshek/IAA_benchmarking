
#include <memory>
#include <unistd.h>

#include <glog/logging.h>

#include "qpl/qpl.h"

namespace qpl_single_engine {

static constexpr qpl_path_t execution_path = qpl_path_hardware;
// static constexpr qpl_path_t execution_path = qpl_path_software;

std::unique_ptr<uint8_t[]> init_qpl() {
  // Job initialization.
  uint32_t job_size = 0;
  qpl_status status = qpl_get_job_size(execution_path, &job_size);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error acquired during job size getting.";
    return std::unique_ptr<uint8_t[]>(nullptr);
  }

  std::unique_ptr<uint8_t[]> job_buffer;
  job_buffer = std::make_unique<uint8_t[]>(job_size);
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  status = qpl_init_job(execution_path, job);
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

int compress(const uint8_t *src, size_t src_size, uint8_t *dst,
             size_t *dst_size) {
  int job_n = 1;
  auto job_buffer = init_qpl();
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // Compress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_compress;
  job->level = qpl_default_level;
  job->next_in_ptr = const_cast<uint8_t *>(src);
  job->next_out_ptr = dst;
  job->available_in = src_size;
  job->available_out = src_size / 2;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_DYNAMIC_HUFFMAN |
               QPL_FLAG_OMIT_VERIFY | QPL_FLAG_LAST;
  qpl_status status = qpl_execute_job(job);
  if (status != QPL_STS_OK) {
    LOG(WARNING) << "An error " << status << " acquired during compression.";
    return -1;
  }
  *dst_size = job->total_out;

  if (free_qpl(job)) {
    LOG(WARNING) << "Failed to free resources.";
    return -1;
  }
  return 0;
}

int decompress(const uint8_t *src, size_t src_size, uint8_t *dst,
               size_t dst_reserved_size, size_t *dst_actual_size) {
  auto job_buffer = init_qpl();
  if (job_buffer == nullptr) {
    LOG(WARNING) << "Failed to init qpl.";
    return -1;
  }

  // Decompress.
  auto job = reinterpret_cast<qpl_job *>(job_buffer.get());
  job->op = qpl_op_decompress;
  job->next_in_ptr = const_cast<uint8_t *>(src);
  job->next_out_ptr = dst;
  job->available_in = src_size;
  job->available_out = dst_reserved_size;
  job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;
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

} // namespace qpl_single_engine
