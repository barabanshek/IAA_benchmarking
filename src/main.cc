#include <iostream>

#include <benchmark/benchmark.h>

#include "multi_engine/benchmark.h"
#include "single_engine/benchmark.h"
#include "single_engine/benchmark_page_faults.h"
#include "single_engine/benchmark_scattered.h"

#include <gflags/gflags.h>

// Benchmarks:
//  - qpl_path_software vs qpl_path_hardware for kModeFixed and kModeDynamic for
//  each benchmarks from corpus;
//  - qpl_path_hardware for kContinious, kNaive, and kCanned for each benchmarks
//  from corpus with 4~kB split for each benchmark;
//  - qpl_path_hardware for kParallelFixed, kParallelDynamic, and
//  kParallelCanned for each benchmark from corpus with job parallezation.
//  - qpl_path_hardware for kMajorPageFaults, kMinorPageFaults, kAtsMiss, and
//  kNoFaults for each benchmark from corpus.
//  - full system (see code).
void register_benchmarks_with_corpus_datasets() {
  static std::map<std::string, std::tuple<uint8_t *, size_t, double>>
      source_buffs;

  CompressionDataset silesia_dataset =
      load_corpus_dataset("dataset/silesia_tmp");
  for (auto const &[mem_size, name, entropy, source_buff] : silesia_dataset) {
    source_buffs[name] = std::make_tuple(source_buff, mem_size, entropy);
  }

  CompressionDataset snapshots_dataset =
      load_corpus_dataset("dataset/snapshots_tmp");
  for (auto const &[mem_size, name, entropy, source_buff] : snapshots_dataset) {
    source_buffs[name] = std::make_tuple(source_buff, mem_size, entropy);
  }

  for (const auto &[benchmark_name, benchmark_data] : source_buffs) {
    auto const [source_buff, mem_size, entropy] = source_buffs[benchmark_name];

    // #1
    qpl_huffman_table_t empty_table;
    for (const auto execution_path : {qpl_path_software, qpl_path_hardware}) {
      for (const auto compression_mode :
           {single_engine::kModeFixed, single_engine::kModeDynamic}) {
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_Compress_" +
                std::to_string(mem_size / kkB) + "kB" + "_name_" +
                benchmark_name + "_entropy_" + std::to_string(entropy) +
                "_mode_" + std::to_string(compression_mode) +
                (execution_path == qpl_path_software ? "_qpl_path_software"
                                                     : "_qpl_path_hardware"),
            single_engine::BM_SingleEngineBlocking_Compress, execution_path,
            static_cast<int>(compression_mode), mem_size, source_buff,
            empty_table);
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_DeCompress_" +
                std::to_string(mem_size / kkB) + "kB" + "_name_" +
                benchmark_name + "_entropy_" + std::to_string(entropy) +
                +"_mode_" + std::to_string(compression_mode) +
                +(execution_path == qpl_path_software ? "_qpl_path_software"
                                                      : "_qpl_path_hardware"),
            single_engine::BM_SingleEngineBlocking_DeCompress, execution_path,
            static_cast<int>(compression_mode), mem_size, source_buff,
            empty_table);
      }
    }

    // #2
    for (const auto compression_mode :
         {single_engine_canned::kContinious, single_engine_canned::kNaive,
          single_engine_canned::kCanned}) {
      benchmark::RegisterBenchmark(
          "BM_SingleEngineBlocking_Compress_Canned_" +
              std::to_string(mem_size / kkB) + "kB" + "_name_" +
              benchmark_name + "_entropy_" + std::to_string(entropy) +
              "_mode_" + std::to_string(compression_mode),
          single_engine::BM_SingleEngineBlocking_CompressCanned,
          static_cast<int>(compression_mode), mem_size, source_buff);
      benchmark::RegisterBenchmark(
          "BM_SingleEngineBlocking_DeCompress_Canned_" +
              std::to_string(mem_size / kkB) + "kB" + "_name_" +
              benchmark_name + "_entropy_" + std::to_string(entropy) +
              "_mode_" + std::to_string(compression_mode),
          single_engine::BM_SingleEngineBlocking_DeCompressCanned,
          static_cast<int>(compression_mode), mem_size, source_buff);
    }

    // #3
    for (const auto compression_mode :
         {multi_engine::kParallelFixed, multi_engine::kParallelDynamic,
          multi_engine::kParallelCanned}) {
      for (const int job_n :
           {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26}) {
        benchmark::RegisterBenchmark(
            "BM_MultipleEngine_Compress_" + std::to_string(mem_size / kkB) +
                "kB" + "_name_" + benchmark_name + "_entropy_" +
                std::to_string(entropy) + "_jobs_" + std::to_string(job_n) +
                "_mode_" + std::to_string(compression_mode),
            multi_engine::BM_MultipleEngine_Compress,
            static_cast<int>(compression_mode), mem_size, job_n, source_buff);
        benchmark::RegisterBenchmark(
            "BM_MultipleEngine_DeCompress_" + std::to_string(mem_size / kkB) +
                "kB" + "_name_" + benchmark_name + "_entropy_" +
                std::to_string(entropy) + "_jobs_" + std::to_string(job_n) +
                "_mode_" + std::to_string(compression_mode),
            multi_engine::BM_MultipleEngine_DeCompress,
            static_cast<int>(compression_mode), mem_size, job_n, source_buff);
      }
    }

    // #4
    for (const auto pf_scenario :
         {page_faults::kMajorPageFaults, page_faults::kMinorPageFaults,
          page_faults::kAtsMiss, page_faults::kNoFaults}) {
      benchmark::RegisterBenchmark(
          "BM_SingleEngineMinorPageFault_Compress_" +
              std::to_string(mem_size / kkB) + "kB" + "_name_" +
              benchmark_name + "_entropy_" + std::to_string(entropy) +
              "_pfscenario_" + std::to_string(pf_scenario),
          page_faults::BM_SingleEngineMinorPageFault_Compress,
          static_cast<int>(pf_scenario), mem_size, source_buff);
      benchmark::RegisterBenchmark(
          "BM_SingleEngineMinorPageFault_DeCompress_" +
              std::to_string(mem_size / kkB) + "kB" + "_name_" +
              benchmark_name + "_entropy_" + std::to_string(entropy) +
              "_pfscenario_" + std::to_string(pf_scenario),
          page_faults::BM_SingleEngineMinorPageFault_DeCompress,
          static_cast<int>(pf_scenario), mem_size, source_buff);
    }
  }

  // Full system benchmark.
  // #5
  CompressionDataset wiki_1GB_dataset = load_corpus_dataset("dataset/wiki_tmp");
  assert(wiki_1GB_dataset.size() == 1);
  static std::map<size_t, std::string> compressed_filenames;
  for (const size_t read_size_ :
       {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
        131072, 262144, 524288}) {
    const auto read_size = read_size_ * kkB;
    compressed_filenames[read_size] =
        std::string("compressfile_") + std::to_string(read_size) + ".dat";
    size_t compressed_size = 0;
    if (page_faults::prepare_compressed_files(
            std::get<3>(wiki_1GB_dataset.front()), read_size,
            compressed_filenames[read_size].c_str(), &compressed_size)) {
      LOG(FATAL) << "Failed to create prepare compressed files.";
    }

    for (auto mode : {page_faults::kBenchmarkDiskRead,
                      page_faults::kBenchmarkDiskReadIODirect,
                      page_faults::kBenchmarkDecompress,
                      page_faults::kBenchmarkDecompressFromFile}) {
      benchmark::RegisterBenchmark(
          "BM_FullSystem_" + std::to_string(read_size / kkB) + "kB" + "_mode_" +
              std::to_string(mode),
          page_faults::BM_FullSystem, static_cast<int>(mode), read_size,
          compressed_filenames[read_size].c_str(), compressed_size);
    }
  }
}

void register_benchmarks() { register_benchmarks_with_corpus_datasets(); }

int main(int argc, char **argv) {
  register_benchmarks();

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
