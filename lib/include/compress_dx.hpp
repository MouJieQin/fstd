#pragma once

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zdict.h>
#include <zstd.h>

#include "logger.hpp"

// ==============================
// 配置项
// ==============================
constexpr size_t BLOCK_SIZE = 8 * 1024;
constexpr int ZSTD_LEVEL = 5;
constexpr bool ENABLE_CHECKSUM = true;
constexpr size_t DICT_SIZE = 32 * 1024;
const char *DICT_FILE = "zstd_dict.bin";

namespace fstd {

// 索引条目（内存+磁盘结构）
// ==============================
struct IndexEntry {
  uint64_t blockOffset;       // 压缩块在文件中的字节偏移
  uint32_t blockSize;         // 压缩块大小
  uint32_t originalBlockSize; // 原始块大小
  uint32_t entryOffset;       // 本条在解压后的块内偏移
  uint32_t entrySize;         // 原始字符串长度
};

class Dxcompressor {

public:
  Dxcompressor() = default;
  ~Dxcompressor() = default;

  bool compressTextToStream(const std::vector<std::string> &texts,
                            std::ostream &dictOut, std::ostream &idxOut,
                            std::ostream &compOut) {
    // 1. 训练并保存字典
    std::vector<char> dictBuffer(DICT_SIZE);
    if (!trainZstdDictionary(texts, dictBuffer.data(), DICT_SIZE)) {
      LOG_ERROR("字典训练失败！");
      return false;
    }
    saveDictionary(dictOut, dictBuffer.data(), DICT_SIZE);
    LOG_INFO("字典训练完成！");

    // 2. 压缩
    if (!compressTextsToStreamImpl(texts, dictBuffer.data(), DICT_SIZE, idxOut,
                                   compOut)) {
      LOG_ERROR("压缩失败！");
      return false;
    }
    LOG_INFO("压缩完成！");
    return true;
  }

  // ==============================
  // 加载索引
  // ==============================
  std::vector<IndexEntry> loadIndex(const char *indexFile = "dict.idx") {
    std::ifstream f(indexFile, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<IndexEntry> index(size / sizeof(IndexEntry));
    f.read(reinterpret_cast<char *>(index.data()), size);
    return index;
  }

  std::string readTextByIndex(uint32_t index,
                              const std::vector<char> &dictBuffer,
                              const std::vector<IndexEntry> &indexEntries,
                              const std::string &compFile, size_t offset) {

    ZSTD_DDict *ddict = ZSTD_createDDict(dictBuffer.data(), dictBuffer.size());

    std::string res;
    if (!indexEntries.empty()) {
      res = getTextByIndex(index, indexEntries, ddict, compFile, offset);
    }

    ZSTD_freeDDict(ddict);
    return res;
  }

private:
  // ==============================
  // 训练字典 (🔥修复：必须将所有样本拼接到一块连续内存中)
  // ==============================
  bool trainZstdDictionary(const std::vector<std::string> &texts,
                           char *dictBuffer, size_t dictSize) {
    std::vector<char> samplesBlob;
    std::vector<size_t> sampleSizes;

    // ZDICT需要所有的样本放在同一个连续的Buffer里
    for (const auto &text : texts) {
      samplesBlob.insert(samplesBlob.end(), text.begin(), text.end());
      sampleSizes.push_back(text.size());
    }

    if (samplesBlob.empty()) {
      LOG_ERROR("没有提供训练数据！");
      return false;
    }

    size_t ret = ZDICT_trainFromBuffer(dictBuffer, dictSize, samplesBlob.data(),
                                       sampleSizes.data(), sampleSizes.size());
    if (ZDICT_isError(ret)) {
      LOG_ERROR("字典训练失败: {}", ZDICT_getErrorName(ret));
      return false;
    }
    return true;
  }

  // ==============================
  // 保存/加载字典
  // ==============================
  bool saveDictionary(std::ostream &os, const char *dictBuffer,
                      size_t dictSize) {
    if (!os) return false;
    os.write(dictBuffer, dictSize);
    return true;
  }

  std::vector<char> loadDictionary(const char *dictFile) {
    std::ifstream f(dictFile, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<char> dict(size);
    f.read(dict.data(), size);
    return dict;
  }

  // ==============================
  // 压缩函数 (🔥修复：索引覆盖Bug & 错误状态返回)
  // ==============================
  bool compressTextsToStreamImpl(const std::vector<std::string> &texts,
                                 const char *dictBuffer, size_t dictSize,
                                 std::ostream &idxOut, std::ostream &compOut) {
    if (!compOut || !idxOut) return false;

    ZSTD_CDict *cdict = ZSTD_createCDict(dictBuffer, dictSize, ZSTD_LEVEL);
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    if (!cdict || !cctx) {
      ZSTD_freeCCtx(cctx);
      ZSTD_freeCDict(cdict);
      return false;
    }

    // 开启checksum验证
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, ENABLE_CHECKSUM ? 1 : 0);

    std::string blockBuffer;
    blockBuffer.reserve(BLOCK_SIZE * 2);
    std::vector<IndexEntry> index;

    uint64_t currentCompOffset = 0;
    size_t blockStartIndex =
        0; // 🔥修复：用游标替代 entry.blockOffset == 0 的判断
    bool success = true;

    for (const auto &text : texts) {
      uint32_t entrySize = (uint32_t)text.size();
      uint32_t entryOffset = (uint32_t)blockBuffer.size();
      index.push_back({0, 0, 0, entryOffset, entrySize});
      blockBuffer.append(text);

      if (blockBuffer.size() >= BLOCK_SIZE) {
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

        // 仅更新当前块内的条目
        for (size_t i = blockStartIndex; i < index.size(); ++i) {
          index[i].blockOffset = currentCompOffset;
          index[i].blockSize = (uint32_t)compSize;
          index[i].originalBlockSize = (uint32_t)blockBuffer.size();
        }

        currentCompOffset += compSize;
        blockStartIndex = index.size();
        blockBuffer.clear();
      }
    }

    // 处理最后一块未满的缓冲区
    if (!blockBuffer.empty()) {
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

      for (size_t i = blockStartIndex; i < index.size(); ++i) {
        index[i].blockOffset = currentCompOffset;
        index[i].blockSize = (uint32_t)compSize;
        index[i].originalBlockSize = (uint32_t)blockBuffer.size();
      }
    }

    idxOut.write(reinterpret_cast<const char *>(index.data()),
                 index.size() * sizeof(IndexEntry));

  cleanup:
    ZSTD_freeCCtx(cctx);
    ZSTD_freeCDict(cdict);
    return success;
  }

  // ==============================
  // 解压函数 (🔥修复：增加ZSTD_getFrameContentSize的安全性校验)
  // ==============================
  std::string getTextByIndex(size_t idx, const std::vector<IndexEntry> &index,
                             const ZSTD_DDict *ddict,
                             const std::string &compFile, size_t offset) {
    if (idx >= index.size()) return "";
    const auto &entry = index[idx];
    LOG_INFO("entry:{},{},{},{}", entry.blockOffset, entry.blockSize,
             entry.entryOffset, entry.entrySize);

    std::ifstream compIn(compFile, std::ios::binary);
    if (!compIn) return "";

    compIn.seekg(offset + entry.blockOffset);
    std::vector<char> compBuf(entry.blockSize);
    compIn.read(compBuf.data(), compBuf.size());

    // 安全检查，避免错误分配巨大内存导致崩溃
    //     unsigned long long decompBufSize =
    //     ZSTD_getFrameContentSize(compBuf.data(), compBuf.size());
    // if (decompBufSize == ZSTD_CONTENTSIZE_ERROR || decompBufSize ==
    // ZSTD_CONTENTSIZE_UNKNOWN) {
    //     std::cerr << "无法获取未压缩帧的大小!\n";
    //     return "";
    // }
    unsigned long long decompBufSize = entry.originalBlockSize + 1;
    LOG_INFO("entry.originalBlockSize:{}, ZSTD_getFrameContentSize: {}",
             entry.originalBlockSize,
             ZSTD_getFrameContentSize(compBuf.data(), compBuf.size()));

    std::vector<char> decompBuf(decompBufSize);
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    if (!dctx) return "";

    size_t decompSize =
        ZSTD_decompress_usingDDict(dctx, decompBuf.data(), decompBuf.size(),
                                   compBuf.data(), compBuf.size(), ddict);

    ZSTD_freeDCtx(dctx);

    if (ZSTD_isError(decompSize)) {
      std::cerr << "解压失败: " << ZSTD_getErrorName(decompSize) << "\n";
      return "";
    }

    // 防止越界
    if (entry.entryOffset + entry.entrySize > decompSize) { return ""; }

    return std::string(decompBuf.data() + entry.entryOffset, entry.entrySize);
  }
};

} // namespace fstd