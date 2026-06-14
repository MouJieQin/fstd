#pragma once
#include <string>
#include <vector>

#include <fstd/common.h>
#include <fstd/thread_pool.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/dynamic_progress.hpp>
#include <zstd.h>

constexpr bool ENABLE_CHECKSUM = true;
namespace fstd {

struct BlockIndex {
  BlockIndex() = default;
  BlockIndex(uint32_t end_entry_index, uint64_t block_offset,
             uint32_t block_size, uint32_t original_block_size);
  uint32_t end_entry_index;
  uint64_t block_offset;
  uint32_t block_size;
  uint32_t original_block_size;
};

struct EntryIndex {
  EntryIndex() = default;
  EntryIndex(uint32_t entry_offset, uint32_t entry_size);
  uint32_t entry_offset;
  uint32_t entry_size;
};

class FstdxCompressor {

public:
  FstdxCompressor() = default;
  ~FstdxCompressor() = default;

  bool compress_texts_to_stream(
      std::ostream &out, const std::vector<std::string> &texts,
      DxJsonHeader &header, size_t dictSize, size_t blockSize,
      int compressionLevel, ThreadPool &thread_pool,
      indicators::DynamicProgress<indicators::BlockProgressBar> &bars);

  std::string readTextByIndex(const uint32_t index, const ZSTD_DDict *ddict,
                              const std::vector<BlockIndex> &blockIndexes,
                              const std::vector<EntryIndex> &entryIndexes,
                              const std::string &compFile,
                              const size_t offset) const;

  std::vector<std::string>
  extract(const std::string &compFile, const size_t offset,
          const ZSTD_DDict *ddict, const std::vector<BlockIndex> &block_indexes,
          const std::vector<EntryIndex> &entry_indexes) const;

private:
  int random_int(int max);

  int trainZstdDictionary(const std::vector<std::string> &texts,
                          char *dictBuffer, size_t dictSize);

  bool compress_texts_to_stream(
      std::ostream &out, const std::vector<std::string> &texts,
      DxJsonHeader &header, const char *dictBuffer, size_t dictSize,
      size_t blockSize, int compressionLevel, ThreadPool &thread_pool,
      indicators::DynamicProgress<indicators::BlockProgressBar> &bars);

  size_t
  bin_search_block_index(uint32_t entry_index,
                         const std::vector<BlockIndex> &block_indexes) const;

  std::string getTextByIndex(const size_t idx,
                             const std::vector<BlockIndex> &block_indexes,
                             const std::vector<EntryIndex> &entry_indexes,
                             const ZSTD_DDict *ddict,
                             const std::string &compFile,
                             const size_t offset) const;

  std::vector<std::string>
  extract_comp_blocks(const std::string &compFile, const size_t offset,
                      const ZSTD_DDict *ddict,
                      const std::vector<BlockIndex> &block_indexes,
                      const std::vector<EntryIndex> &entry_indexes) const;

private:
  // 全局队列 & 同步
  std::queue<CompressTask> task_queue_;
  std::queue<CompressResult> result_queue_;
  std::mutex task_mtx_;
  std::mutex res_mtx_;
  std::condition_variable task_cv_;
  std::condition_variable res_cv_;

  std::atomic<size_t> task_seq_{0}; // 分配任务的全局序号
  std::atomic<bool> success_{true};
  std::atomic<bool> generate_finished_{false};
  std::atomic<bool> compress_finished_{false};
};

} // namespace fstd