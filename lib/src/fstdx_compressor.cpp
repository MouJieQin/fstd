
#include <fstream>
#include <iostream>
#include <random>
#include <zdict.h>

#include <fstd/common.h>
#include <fstd/fstdx_compressor.h>
#include <fstd/logger.h>

using namespace indicators;
using namespace std;
namespace fstd {

BlockIndex::BlockIndex(uint32_t end_entry_index, uint64_t block_offset,
                       uint32_t block_size, uint32_t original_block_size)
    : end_entry_index(end_entry_index), block_offset(block_offset),
      block_size(block_size), original_block_size(original_block_size) {}

EntryIndex::EntryIndex(uint32_t entry_offset, uint32_t entry_size)
    : entry_offset(entry_offset), entry_size(entry_size) {}

bool FstdxCompressor::compress_texts_to_stream(
    std::ostream &out, const std::vector<std::string> &texts,
    DxJsonHeader &header, size_t dictSize, size_t blockSize,
    int compressionLevel, ThreadPool &thread_pool,
    DynamicProgress<BlockProgressBar> &bars) {
  // 1. 训练并保存字典
  std::vector<char> dictBuffer(dictSize);
  dictSize = trainZstdDictionary(texts, dictBuffer.data(), dictSize);
  if (dictSize == -1) {
    LOG_ERROR("Train Zstd dictionary failed!");
    return false;
  }
  LOG_INFO("Train Zstd dictionary success!");

  // 2. 压缩
  if (!compress_texts_to_stream(out, texts, header, dictBuffer.data(), dictSize,
                                blockSize, compressionLevel, thread_pool,
                                bars)) {
    LOG_ERROR("Compress texts to stream failed!");
    return false;
  }
  LOG_INFO("Compress texts to stream success!");
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

int FstdxCompressor::trainZstdDictionary(const std::vector<std::string> &texts,
                                         char *dictBuffer, size_t dictSize) {
  if (texts.empty()) {
    LOG_ERROR("No training data provided!");
    return -1;
  }
  const size_t total_sample_byte_size = dictSize * 100;
  std::vector<char> samplesBlob;
  std::vector<size_t> sampleSizes;

  LOG_DEBUG("total_sample_byte_size:{}", total_sample_byte_size);
  while (samplesBlob.size() < total_sample_byte_size) {
    int random_index = random_int(texts.size());
    samplesBlob.insert(samplesBlob.end(), texts[random_index].begin(),
                       texts[random_index].end());
    sampleSizes.push_back(texts[random_index].size());
  }

  LOG_INFO("Train Zstd dictionary ...");
  size_t ret_size =
      ZDICT_trainFromBuffer(dictBuffer, dictSize, samplesBlob.data(),
                            sampleSizes.data(), sampleSizes.size());
  if (ZDICT_isError(ret_size)) {
    LOG_ERROR("Train Zstd dictionary failed: {}", ZDICT_getErrorName(ret_size));
    return -1;
  }
  return ret_size;
}

bool FstdxCompressor::compress_texts_to_stream(
    std::ostream &out, const std::vector<std::string> &texts,
    DxJsonHeader &header, const char *dictBuffer, size_t dictSize,
    size_t blockSize, int compressionLevel, ThreadPool &thread_pool,
    DynamicProgress<BlockProgressBar> &bars) {

  ZSTD_CDict *cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
  if (!cdict) { return false; }
  std::vector<EntryIndex> entry_indexes;
  entry_indexes.reserve(texts.size());

  std::vector<std::pair<size_t, size_t>> block_record;
  size_t block_buf_size = 0;
  for (size_t i = 0; i < texts.size(); ++i) {
    uint32_t entry_size = (uint32_t)texts[i].size();
    uint32_t entry_offset = (uint32_t)block_buf_size;
    entry_indexes.emplace_back(entry_offset, entry_size);
    block_buf_size += texts[i].size();
    if (block_buf_size >= blockSize) {
      block_record.emplace_back(block_buf_size, i + 1);
      block_buf_size = 0;
    }
  }
  if (block_buf_size != 0) {
    block_record.emplace_back(block_buf_size, texts.size());
  }

  auto block_generator = [&]() {
    for (size_t i = 0, j = 0; i < block_record.size(); i++) {
      CompressTask task;
      task.src_data.resize(block_record[i].first);
      size_t offset = 0;
      for (; j < block_record[i].second; ++j) {
        // LOG_INFO("j:{}", j);
        memcpy(task.src_data.data() + offset, texts[j].data(), texts[j].size());
        offset += texts[j].size();
      }
      // if(j!=texts.size()){
      //   LOG_INFO("texts[j-1]:{}, texts[j]:{}", texts[j-1], texts[j]);
      // }
      task.index = i;
      // 队列控流：防止内存无限上涨
      unique_lock<mutex> lock(task_mtx_);
      task_cv_.wait(lock, [&] { return task_queue_.size() < max_queue_size; });
      LOG_INFO("[Block Generator] task_queue_.size():{}", task_queue_.size());
      task_queue_.push(std::move(task));
      task_cv_.notify_one();
    }
    // 标记读取流结束
    unique_lock<mutex> lock(task_mtx_);
    generate_finished_ = true;
    task_cv_.notify_all();
  };

  const bool show_progress = is_terminal();
  auto compress_worker = [&]() {
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    if (!cctx) { return false; }
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, ENABLE_CHECKSUM ? 1 : 0);
    while (true) {
      CompressTask task;
      {
        unique_lock<mutex> lock(task_mtx_);
        task_cv_.wait(
            lock, [&] { return !task_queue_.empty() || generate_finished_; });

        if (task_queue_.empty() && generate_finished_) {
          break; // 没有任务且读取完毕，退出线程
        }
        task = std::move(task_queue_.front());
        task_queue_.pop();
        task_cv_.notify_one(); // 通知读取线程队列有空位了
      }

      // 执行 CPU 密集的压缩计算
      CompressResult result;
      result.index = task.index;
      auto compress = [&]() {
        size_t last_progress = 0;

        // // progress bar
        // auto progress_comp_block = [&](const size_t index,
        //                                const size_t block_size,
        //                                const size_t bar_idx) {
        //   size_t count = index + 1;
        //   if (show_progress) {
        //     size_t progress = count * 100 / block_size;
        //     if (progress > last_progress) {
        //       bars[bar_idx].set_option(option::PostfixText{
        //           std::to_string(count) + "/" + std::to_string(block_size)});
        //       bars[bar_idx].set_progress(progress);
        //       last_progress = progress;
        //     }
        //     if (count == block_size) { bars[bar_idx].mark_as_completed(); }
        //   }
        // };

        if (!success_) {
          // compression failed in other workers
          ZSTD_freeCCtx(cctx);
          return false;
        }
        size_t comp_dst_size = ZSTD_compressBound(task.src_data.size());
        result.dst_data.resize(comp_dst_size);
        size_t comp_size = ZSTD_compress_usingCDict(
            cctx, result.dst_data.data(), result.dst_data.size(),
            task.src_data.data(), task.src_data.size(), cdict);

        if (ZSTD_isError(comp_size)) {
          success_ = false;
          ZSTD_freeCCtx(cctx);
          LOG_ERROR("Compress blocks failed: {}", ZSTD_getErrorName(comp_size));
          return false;
        }
        result.dst_data.resize(comp_size);
        // progress_comp_block(i - begin_idx + 1, end_idx - begin_idx, bar_idx);
        {
          unique_lock<mutex> lock(res_mtx_);
          result_queue_.push(std::move(result));
          res_cv_.notify_one();
        }
      };
      compress();
    };
    ZSTD_freeCCtx(cctx);
    return true;
  };

  uint64_t total_block_size = 0;
  std::vector<BlockIndex> block_indexes;
  auto block_writer = [&]() {
    size_t expected_index = 0;
    // 使用缓存区存放乱序到达的压缩块 (Key: index, Value: Compressed Data)
    unordered_map<size_t, vector<char>> fallback_buffer;
    uint64_t block_offset = 0;

    while (true) {
      CompressResult result;
      {
        unique_lock<mutex> lock(res_mtx_);
        res_cv_.wait(
            lock, [&] { return !result_queue_.empty() || compress_finished_; });

        if (result_queue_.empty() && compress_finished_ &&
            fallback_buffer.empty()) {
          break;
        }

        if (!result_queue_.empty()) {
          result = std::move(result_queue_.front());
          result_queue_.pop();
        }
      }

      // 如果取到了新数据，放入缓存区
      if (!result.dst_data.empty() || result.index == expected_index) {
        fallback_buffer[result.index] = std::move(result.dst_data);
      }

      // 核心：严格按序号循环写入，直到缺憾后续块为止
      while (fallback_buffer.count(expected_index)) {
        auto &data = fallback_buffer[expected_index];

        // 写入元数据头 (数据流块大小)，以便后续解压程序定位块边界
        uint64_t block_size = static_cast<uint64_t>(data.size());

        block_indexes.emplace_back(block_record[expected_index].second,
                                   block_offset, block_size,
                                   block_record[expected_index].first);
        block_offset += block_size;

        // 写入压缩数据主体
        out.write(data.data(), block_size);
        total_block_size += block_size;

        fallback_buffer.erase(expected_index);
        expected_index++;
      }
    }
  };

  thread generator(block_generator);

  size_t worker_size = thread_pool.worker_num();
  vector<future<bool>> results;
  for (size_t i = 0; i < worker_size - 1; ++i) {
    results.emplace_back(thread_pool.enqueue(compress_worker));
  }

  thread writer(block_writer);

  generator.join();

  for (auto &res : results) {
    if (!res.get()) { success_ = false; }
  }

  if (!success_) {
    ZSTD_freeCDict(cdict);
    return false;
  }
  LOG_INFO("Compress done.");

  // 5. 标记压缩完全结束，通知并回收写入线程
  {
    unique_lock<mutex> lock(res_mtx_);
    compress_finished_ = true;
    res_cv_.notify_all();
  }
  writer.join();

  header["comp_blocks"]["compress_level"] = compressionLevel;
  header["comp_blocks"]["offset"] = 0;

  out.write(dictBuffer, dictSize);
  header["comp_dict"]["compress_level"] = 0;
  header["comp_dict"]["original_size"] = dictSize;
  header["comp_dict"]["offset"] =
      static_cast<size_t>(header["comp_blocks"]["offset"]) + total_block_size;

  bool comp_res = false;
  size_t comp_block_index_size = 0;
  {
    std::vector<char> comp_block_index_dst;
    comp_res =
        compress_to_buffer(reinterpret_cast<const char *>(block_indexes.data()),
                           block_indexes.size() * sizeof(BlockIndex),
                           comp_block_index_dst, compressionLevel);
    if (!comp_res) { return false; }
    out.write(comp_block_index_dst.data(), comp_block_index_dst.size());
    comp_block_index_size = comp_block_index_dst.size();
    header["block_indexes"]["compress_level"] = compressionLevel;
    header["block_indexes"]["compressed_size"] = comp_block_index_size;
    header["block_indexes"]["original_size"] =
        block_indexes.size() * sizeof(BlockIndex);
    header["block_indexes"]["offset"] =
        static_cast<size_t>(header["comp_dict"]["offset"]) + dictSize;
  }

  {
    out.write(reinterpret_cast<const char *>(entry_indexes.data()),
              entry_indexes.size() * sizeof(EntryIndex));
    header["entry_indexes"]["compress_level"] = 0;
    header["entry_indexes"]["original_size"] =
        entry_indexes.size() * sizeof(EntryIndex);
    header["entry_indexes"]["offset"] =
        static_cast<size_t>(header["block_indexes"]["offset"]) +
        comp_block_index_size;
  }

  ZSTD_freeCDict(cdict);
  return true;
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

  // LOG_INFO("block_index.end_entry_index: {}, block_index.block_offset: {}, "
  //          "block_index.block_size: {}, block_index.original_block_size: {}",
  //          block_index.end_entry_index, block_index.block_offset,
  //          block_index.block_size, block_index.original_block_size);
  // LOG_INFO("entry_index.entry_offset: {}, entry_index.entry_size: {}",
  //          entry_index.entry_offset, entry_index.entry_size);

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