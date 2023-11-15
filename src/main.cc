#include <iostream>

#include <benchmark/benchmark.h>

#include "multi_engine/benchmark.h"
#include "single_engine/benchmark.h"
#include "single_engine/benchmark_page_faults.h"

void register_benchmarks() {
  // Setup.
  const std::vector<uint16_t> kEntropyList = {1,   5,   10,  25, 50,
                                              150, 200, 300, 400};
  const std::vector<size_t> kMemorySizeList = {512 * kMB, 256 * kMB, 64 * kMB,
                                               16 * kMB,  1 * kMB,   256 * kkB,
                                               64 * kkB,  4 * kkB};
  //   const std::vector<uint16_t> kEntropyList = {400};
  //   const std::vector<size_t> kMemorySizeList = {512 * kMB};

  // Register memory.
  static std::map<std::tuple<uint16_t, size_t>, std::tuple<uint8_t *, double>>
      source_buffs;
  static std::map<uint16_t, qpl_huffman_table_t> huffman_tables;
  static std::map<size_t, std::string> compressed_filenames;
  for (const auto entropy : kEntropyList) {
    for (const size_t &mem_size : kMemorySizeList) {
      uint8_t *source_buff = reinterpret_cast<uint8_t *>(malloc(mem_size));
      double true_entropy = init_rand_memory(source_buff, mem_size, entropy);
      source_buffs[std::make_tuple(entropy, mem_size)] =
          std::make_tuple(source_buff, true_entropy);
    }

    // Make Huffman tables for different entropy levels.
    size_t max_mem_size =
        *std::max_element(kMemorySizeList.begin(), kMemorySizeList.end());
    qpl_huffman_table_t huffman_table;
    auto src =
        std::get<0>(source_buffs[std::make_tuple(entropy, max_mem_size)]);
    if (create_static_huffman_tables(qpl_path_hardware, &huffman_table, src,
                                     max_mem_size)) {
      LOG(FATAL) << "Failed to create Huffman tables";
    }
    huffman_tables[entropy] = huffman_table;
  }

  // Register benchmarks.
  for (const auto entropy : kEntropyList) {
    for (const size_t &mem_size : kMemorySizeList) {
      auto source_buff =
          std::get<0>(source_buffs[std::make_tuple(entropy, mem_size)]);
      auto true_entropy =
          std::get<1>(source_buffs[std::make_tuple(entropy, mem_size)]);

      for (const auto execution_path : {qpl_path_software, qpl_path_hardware}) {
        for (const auto compression_mode :
             {single_engine::kModeFixed, single_engine::kModeDynamic,
              single_engine::kModeStatic}) {
          benchmark::RegisterBenchmark(
              "BM_SingleEngineBlocking_Compress_" +
                  std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                  std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                  +"_mode_" + std::to_string(compression_mode) +
                  (execution_path == qpl_path_software ? "_qpl_path_software"
                                                       : "_qpl_path_hardware"),
              single_engine::BM_SingleEngineBlocking_Compress, execution_path,
              static_cast<int>(compression_mode), mem_size, source_buff,
              huffman_tables[entropy]);
          benchmark::RegisterBenchmark(
              "BM_SingleEngineBlocking_DeCompress_" +
                  std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                  std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                  +"_mode_" + std::to_string(compression_mode) +
                  +(execution_path == qpl_path_software ? "_qpl_path_software"
                                                        : "_qpl_path_hardware"),
              single_engine::BM_SingleEngineBlocking_DeCompress, execution_path,
              static_cast<int>(compression_mode), mem_size, source_buff,
              huffman_tables[entropy]);
        }
      }

      for (const auto compression_mode :
           {single_engine_canned::kContinious, single_engine_canned::kNaive,
            single_engine_canned::kCanned}) {
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_Compress_Canned_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                "_mode_" + std::to_string(compression_mode),
            single_engine::BM_SingleEngineBlocking_CompressCanned,
            static_cast<int>(compression_mode), mem_size, source_buff);
        benchmark::RegisterBenchmark(
            "BM_SingleEngineBlocking_DeCompress_Canned_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                "_mode_" + std::to_string(compression_mode),
            single_engine::BM_SingleEngineBlocking_DeCompressCanned,
            static_cast<int>(compression_mode), mem_size, source_buff);
      }

      for (const auto compression_mode :
           {multi_engine::kParallelDynamic, multi_engine::kParallelCanned}) {
        for (const int job_n :
             {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}) {
          benchmark::RegisterBenchmark(
              "BM_MultipleEngine_Compress_" + std::to_string(mem_size / kkB) +
                  "kB" + "_entropy_" + std::to_string(entropy) + "_" +
                  std::to_string(true_entropy) + "_jobs_" +
                  std::to_string(job_n) + "_mode_" +
                  std::to_string(compression_mode),
              multi_engine::BM_MultipleEngine_Compress,
              static_cast<int>(compression_mode), mem_size, job_n, source_buff);
          benchmark::RegisterBenchmark(
              "BM_MultipleEngine_DeCompress_" + std::to_string(mem_size / kkB) +
                  "kB" + "_entropy_" + std::to_string(entropy) + "_" +
                  std::to_string(true_entropy) + "_jobs_" +
                  std::to_string(job_n) + "_mode_" +
                  std::to_string(compression_mode),
              multi_engine::BM_MultipleEngine_DeCompress,
              static_cast<int>(compression_mode), mem_size, job_n, source_buff);
        }
      }

      for (const auto pf_scenario :
           {page_faults::kMajorPageFaults, page_faults::kMinorPageFaults,
            page_faults::kAtsMiss, page_faults::kNoFaults}) {
        benchmark::RegisterBenchmark(
            "BM_SingleEngineMinorPageFault_Compress_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                "_pfscenario_" + std::to_string(pf_scenario),
            page_faults::BM_SingleEngineMinorPageFault_Compress,
            static_cast<int>(pf_scenario), mem_size, source_buff);
        benchmark::RegisterBenchmark(
            "BM_SingleEngineMinorPageFault_DeCompress_" +
                std::to_string(mem_size / kkB) + "kB" + "_entropy_" +
                std::to_string(entropy) + "_" + std::to_string(true_entropy) +
                "_pfscenario_" + std::to_string(pf_scenario),
            page_faults::BM_SingleEngineMinorPageFault_DeCompress,
            static_cast<int>(pf_scenario), mem_size, source_buff);
      }
    }
  }

  // Register full-system benchmarks.
  for (const size_t &mem_size : kMemorySizeList) {
    auto entropy = kEntropyList.back();
    auto source_buff =
        std::get<0>(source_buffs[std::make_tuple(entropy, mem_size)]);
    compressed_filenames[mem_size] =
        std::string("compressfile_") + std::to_string(mem_size) + ".dat";
    if (page_faults::prepare_compressed_files(
            source_buff, mem_size, compressed_filenames[mem_size].c_str())) {
      LOG(FATAL) << "Failed to create prepare compressed files.";
    }

    for (auto mode :
         {page_faults::kBenchmarkDiskRead, page_faults::kBenchmarkDecompress,
          page_faults::kBenchmarkDecompressFromFile}) {
      benchmark::RegisterBenchmark(
          "BM_FullSystem_" + std::to_string(mem_size / kkB) + "kB" + "_mode_" +
              std::to_string(mode),
          page_faults::BM_FullSystem, static_cast<int>(mode), mem_size,
          compressed_filenames[mem_size].c_str());
    }
  }
}

int main(int argc, char **argv) {
  register_benchmarks();

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
