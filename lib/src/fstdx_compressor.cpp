
#include <fstream>
#include <iostream>
#include <random>
#include <zdict.h>

#include <fstd/fstdx_compressor.h>
#include <fstd/logger.h>

namespace fstd {

BlockIndex::BlockIndex(uint32_t end_entry_index, uint32_t block_offset,
                       uint32_t block_size, uint32_t original_block_size)
    : end_entry_index(end_entry_index), block_offset(block_offset),
      block_size(block_size), original_block_size(original_block_size) {}

EntryIndex::EntryIndex(uint32_t entry_offset, uint32_t entry_size)
    : entry_offset(entry_offset), entry_size(entry_size) {}

bool FstdxCompressor::compressTextToStream(
    const std::vector<std::string> &texts, std::ostream &dictOut,
    std::ostream &blockIdxOut, std::ostream &entryIdxOut, std::ostream &compOut,
    size_t dictSize, size_t blockSize, int compressionLevel) {
  // 1. 训练并保存字典
  std::vector<char> dictBuffer(dictSize);
  if (!trainZstdDictionary(texts, dictBuffer.data(), dictSize)) {
    LOG_ERROR("字典训练失败！");
    return false;
  }
  saveDictionary(dictOut, dictBuffer.data(), dictSize);

  // 2. 压缩
  if (!compressTextsToStreamImpl(texts, dictBuffer.data(), dictSize,
                                 blockIdxOut, entryIdxOut, compOut, blockSize,
                                 compressionLevel)) {
    LOG_ERROR("压缩失败！");
    return false;
  }
  LOG_INFO("压缩完成！");
  return true;
}

bool FstdxCompressor::compressToBuffer(const std::string &src, size_t srcSize,
                                       std::vector<char> &dst,
                                       int compressionLevel) {
  return compressToBuffer(src.c_str(), srcSize, dst, compressionLevel);
}

bool FstdxCompressor::compressToBuffer(const char *src, size_t srcSize,
                                       std::vector<char> &dst,
                                       int compressionLevel) {
  size_t compBufSize = ZSTD_compressBound(srcSize);
  std::vector<char> compBuf(compBufSize);
  size_t compSize = ZSTD_compress(compBuf.data(), compBuf.size(), src, srcSize,
                                  compressionLevel);
  if (ZSTD_isError(compSize)) {
    LOG_ERROR("压缩失败: {}", ZSTD_getErrorName(compSize));
    return false;
  }
  compBuf.resize(compSize);
  dst.swap(compBuf);
  return true;
}

bool FstdxCompressor::decompressToBuffer(const void *src, size_t compressedSize,
                                         size_t originalSize,
                                         std::vector<char> &dst) {
  std::vector<char> decomp_buf(originalSize);
  size_t actual_decomp_size =
      ZSTD_decompress(decomp_buf.data(), originalSize, src, compressedSize);
  if (ZSTD_isError(actual_decomp_size)) {
    LOG_ERROR("解压失败: {}", ZSTD_getErrorName(actual_decomp_size));
    return false;
  }
  dst.swap(decomp_buf);
  return true;
}

std::string
FstdxCompressor::readTextByIndex(const uint32_t index, const ZSTD_DDict *ddict,
                                 const std::vector<BlockIndex> &blockIndexes,
                                 const std::vector<EntryIndex> &entryIndexes,
                                 const std::string &compFile,
                                 const size_t offset) const {

  if (blockIndexes.empty() || entryIndexes.empty()) { return ""; }

  std::string res = getTextByIndex(index, blockIndexes, entryIndexes, ddict,
                                   compFile, offset);

  return res;
}

std::vector<std::string>
FstdxCompressor::extract(const std::string &compFile, const size_t offset,
                         const ZSTD_DDict *ddict,
                         const std::vector<BlockIndex> &block_indexes,
                         const std::vector<EntryIndex> &entry_indexes) const

{
  return extract_comp_blocks(compFile, offset, ddict, block_indexes,
                             entry_indexes);
}

int FstdxCompressor::random_int(int max) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // [0, max)
  std::uniform_int_distribution<> dist(0, max - 1);
  return dist(gen);
}

bool FstdxCompressor::trainZstdDictionary(const std::vector<std::string> &texts,
                                          char *dictBuffer, size_t dictSize) {
  if (texts.empty()) {
    LOG_ERROR("没有提供训练数据！");
    return false;
  }
  const size_t total_sample_byte_size = dictSize * 100;
  std::vector<char> samplesBlob;
  std::vector<size_t> sampleSizes;

  LOG_INFO("total_sample_byte_size:{}", total_sample_byte_size);
  while (samplesBlob.size() < total_sample_byte_size) {
    int random_index = random_int(texts.size());
    samplesBlob.insert(samplesBlob.end(), texts[random_index].begin(),
                       texts[random_index].end());
    sampleSizes.push_back(texts[random_index].size());
  }

  LOG_INFO("字典训练 ...");
  size_t ret = ZDICT_trainFromBuffer(dictBuffer, dictSize, samplesBlob.data(),
                                     sampleSizes.data(), sampleSizes.size());
  if (ZDICT_isError(ret)) {
    LOG_ERROR("字典训练失败: {}", ZDICT_getErrorName(ret));
    return false;
  }
  return true;
}

bool FstdxCompressor::saveDictionary(std::ostream &os, const char *dictBuffer,
                                     size_t dictSize) {
  if (!os) return false;
  os.write(dictBuffer, dictSize);
  return true;
}

bool FstdxCompressor::compressTextsToStreamImpl(
    const std::vector<std::string> &texts, const char *dictBuffer,
    size_t dictSize, std::ostream &blockIdxOut, std::ostream &entryIdxOut,
    std::ostream &compOut, size_t blockSize, int compressionLevel) const {
  if (!blockIdxOut || !entryIdxOut || !compOut) return false;

  ZSTD_CDict *cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
  ZSTD_CCtx *cctx = ZSTD_createCCtx();
  if (!cdict || !cctx) {
    ZSTD_freeCCtx(cctx);
    ZSTD_freeCDict(cdict);
    return false;
  }

  ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, ENABLE_CHECKSUM ? 1 : 0);

  std::string blockBuffer;
  blockBuffer.reserve(blockSize * 2);
  std::vector<BlockIndex> block_indexes;
  std::vector<EntryIndex> entry_indexes;
  entry_indexes.reserve(texts.size());

  uint64_t currentCompOffset = 0;
  size_t blockStartIndex = 0;
  bool success = true;

  for (uint32_t i = 0; i < texts.size(); ++i) {
    const std::string &text = texts[i];
    uint32_t entry_size = (uint32_t)text.size();
    uint32_t entry_offset = (uint32_t)blockBuffer.size();
    entry_indexes.emplace_back(entry_offset, entry_size);
    blockBuffer.append(text);

    if (blockBuffer.size() >= blockSize) {
      size_t compBufSize = ZSTD_compressBound(blockBuffer.size());
      std::vector<char> compBuf(compBufSize);

      size_t compSize = ZSTD_compress_usingCDict(
          cctx, compBuf.data(), compBuf.size(), blockBuffer.data(),
          blockBuffer.size(), cdict);

      if (ZSTD_isError(compSize)) {
        success = false;
        goto cleanup;
      }

      compOut.write(compBuf.data(), compSize);

      blockStartIndex = i + 1;
      block_indexes.emplace_back(blockStartIndex, currentCompOffset,
                                 (uint32_t)compSize,
                                 (uint32_t)blockBuffer.size());

      currentCompOffset += compSize;
      blockBuffer.clear();
    }
  }

  // 处理最后一块未满的缓冲区
  if (!blockBuffer.empty()) {
    size_t compBufSize = ZSTD_compressBound(blockBuffer.size());
    std::vector<char> compBuf(compBufSize);

    size_t compSize =
        ZSTD_compress_usingCDict(cctx, compBuf.data(), compBuf.size(),
                                 blockBuffer.data(), blockBuffer.size(), cdict);

    if (ZSTD_isError(compSize)) {
      success = false;
      goto cleanup;
    }

    compOut.write(compBuf.data(), compSize);

    block_indexes.emplace_back(texts.size(), currentCompOffset,
                               (uint32_t)compSize,
                               (uint32_t)blockBuffer.size());
  }

  blockIdxOut.write(reinterpret_cast<const char *>(block_indexes.data()),
                    block_indexes.size() * sizeof(BlockIndex));
  entryIdxOut.write(reinterpret_cast<const char *>(entry_indexes.data()),
                    entry_indexes.size() * sizeof(EntryIndex));

cleanup:
  ZSTD_freeCCtx(cctx);
  ZSTD_freeCDict(cdict);
  return success;
}

size_t FstdxCompressor::bin_search_block_index(
    uint32_t entry_index, const std::vector<BlockIndex> &block_indexes) const {
  size_t first = 0;
  size_t last = block_indexes.size();
  size_t len = last - first;
  while (len > 1) {
    size_t half = len >> 1;
    size_t middle = first + half - 1;
    uint32_t middle_end_index = block_indexes[middle].end_entry_index;
    if (entry_index == middle_end_index) {
      return middle + 1;
    } else if (entry_index < middle_end_index) {
      last = middle + 1;
    } else {
      first = middle + 1;
    }
    len = last - first;
  }
  return first;
}

std::string FstdxCompressor::getTextByIndex(
    const size_t idx, const std::vector<BlockIndex> &block_indexes,
    const std::vector<EntryIndex> &entry_indexes, const ZSTD_DDict *ddict,
    const std::string &compFile, const size_t offset) const {
  std::ifstream compIn(compFile, std::ios::binary);
  if (!compIn) {
    LOG_ERROR("Couldn't open file: {}", compFile);
    return "";
  }
  if (idx >= entry_indexes.size()) {
    LOG_ERROR("Entry index is out of range({}): {}", entry_indexes.size(), idx);
    return "";
  }
  const EntryIndex &entry_index = entry_indexes[idx];

  size_t block_index_idx = bin_search_block_index(idx, block_indexes);
  const BlockIndex &block_index = block_indexes[block_index_idx];

  compIn.seekg(offset + block_index.block_offset);
  std::vector<char> compBuf(block_index.block_size);
  compIn.read(compBuf.data(), compBuf.size());

  unsigned long long decompBufSize = block_index.original_block_size;

  std::vector<char> decompBuf(decompBufSize);
  ZSTD_DCtx *dctx = ZSTD_createDCtx();
  if (!dctx) {
    LOG_ERROR("Create Decompression context failed.");
    return "";
  }

  size_t decompSize =
      ZSTD_decompress_usingDDict(dctx, decompBuf.data(), decompBuf.size(),
                                 compBuf.data(), compBuf.size(), ddict);

  ZSTD_freeDCtx(dctx);

  if (ZSTD_isError(decompSize)) {
    LOG_ERROR("解压失败: {}", ZSTD_getErrorName(decompSize));
    return "";
  }

  return std::string(decompBuf.data() + entry_index.entry_offset,
                     entry_index.entry_size);
}

std::vector<std::string> FstdxCompressor::extract_comp_blocks(
    const std::string &compFile, const size_t offset, const ZSTD_DDict *ddict,
    const std::vector<BlockIndex> &block_indexes,
    const std::vector<EntryIndex> &entry_indexes) const {
  std::vector<std::string> result;
  std::ifstream compIn(compFile, std::ios::binary);
  if (!compIn) {
    LOG_ERROR("Couldn't open file: {}", compFile);
    return result;
  }

  ZSTD_DCtx *dctx = ZSTD_createDCtx();
  if (!dctx) {
    LOG_ERROR("Create Decompression context failed.");
    return result;
  }

  size_t idx = 0;
  std::vector<char> compBuf;
  std::vector<char> decompBuf;
  result.reserve(entry_indexes.size());
  for (const BlockIndex &block_index : block_indexes) {
    compIn.seekg(offset + block_index.block_offset);
    compBuf.resize(block_index.block_size);
    compIn.read(compBuf.data(), compBuf.size());

    decompBuf.resize(block_index.original_block_size);
    size_t decompSize =
        ZSTD_decompress_usingDDict(dctx, decompBuf.data(), decompBuf.size(),
                                   compBuf.data(), compBuf.size(), ddict);
    if (ZSTD_isError(decompSize)) {
      ZSTD_freeDCtx(dctx);
      LOG_ERROR("Decompression failed: {}", ZSTD_getErrorName(decompSize));
      return {};
    }

    for (; idx < block_index.end_entry_index; ++idx) {
      const char *data_ptr = decompBuf.data();
      const EntryIndex &entry_index = entry_indexes[idx];
      result.emplace_back(std::string(data_ptr + entry_index.entry_offset,
                                      entry_index.entry_size));
    }
  }
  ZSTD_freeDCtx(dctx);
  return result;
}

} // namespace fstd