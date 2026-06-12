#pragma once
#include <string>
#include <vector>

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

  bool compressTextToStream(
      const std::vector<std::string> &texts, std::ostream &dictOut,
      std::ostream &blockIdxOut, std::ostream &entryIdxOut,
      std::ostream &compOut, size_t dictSize, size_t blockSize,
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

  bool trainZstdDictionary(const std::vector<std::string> &texts,
                           char *dictBuffer, size_t dictSize);

  bool saveDictionary(std::ostream &os, const char *dictBuffer,
                      size_t dictSize);

  std::vector<char> loadDictionary(const char *dictFile);

  bool compressTextsToStreamImpl(
      const std::vector<std::string> &texts, const char *dictBuffer,
      size_t dictSize, std::ostream &blockIdxOut, std::ostream &entryIdxOut,
      std::ostream &compOut, size_t blockSize, int compressionLevel,
      ThreadPool &thread_pool,
      indicators::DynamicProgress<indicators::BlockProgressBar> &bars) const;

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
};

} // namespace fstd