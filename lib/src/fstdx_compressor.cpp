
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
bool FstdxCompressor::compress_texts_to_stream(
    std::ostream &out, const std::vector<std::string> &texts,
    DxJsonHeader &header, size_t dict_size, size_t block_size,
    int compression_level, ThreadPool &thread_pool,
    DyBlockProgBars &dy_bars) {
  std::vector<char> dict_buffer(dict_size);
  int res = train_zstd_dictionary(texts, dict_buffer.data(), dict_size);
  if (res == -1) {
    LOG_ERROR("Train Zstd dictionary failed!");
    return false;
  }
  LOG_DEBUG("Train Zstd dictionary success!");

  if (!compress_texts_to_stream(out, texts, header, dict_buffer.data(),
                                dict_size, block_size, compression_level,
                                thread_pool, dy_bars)) {
    LOG_ERROR("Compress texts to stream failed!");
    return false;
  }
  return true;
}

int FstdxCompressor::random_int(int max) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // [0, max)
  std::uniform_int_distribution<> dist(0, max - 1);
  return dist(gen);
}

int FstdxCompressor::train_zstd_dictionary(
    const std::vector<std::string> &texts, char *dict_buffer,
    size_t dict_size) {
  if (texts.empty()) {
    LOG_ERROR("No training data provided!");
    return -1;
  }
  const size_t total_sample_byte_size = dict_size * 100;
  std::vector<char> samples_blob;
  std::vector<size_t> sample_sizes;

  while (samples_blob.size() < total_sample_byte_size) {
    int random_index = random_int(texts.size());
    samples_blob.insert(samples_blob.end(), texts[random_index].begin(),
                        texts[random_index].end());
    sample_sizes.push_back(texts[random_index].size());
  }

  LOG_INFO("Train Zstd dictionary ...");
  size_t ret_size =
      ZDICT_trainFromBuffer(dict_buffer, dict_size, samples_blob.data(),
                            sample_sizes.data(), sample_sizes.size());
  if (ZDICT_isError(ret_size)) {
    LOG_ERROR("Train Zstd dictionary failed: {}", ZDICT_getErrorName(ret_size));
    return -1;
  }
  return ret_size;
}

bool FstdxCompressor::block_generator(
    const std::vector<std::string> &texts,
    const std::vector<std::pair<size_t, size_t>> &block_record) {
  for (size_t i = 0, j = 0; i < block_record.size(); i++) {
    CompressTask task;
    task.src_data.resize(block_record[i].first);
    size_t offset = 0;
    for (; j < block_record[i].second; ++j) {
      memcpy(task.src_data.data() + offset, texts[j].data(), texts[j].size());
      offset += texts[j].size();
    }
    task.index = i;
    // To avoid using too much memory, we limit the size of the task queue
    unique_lock<mutex> lock(task_mtx_);
    task_cv_.wait(
        lock, [&] { return !success_ || task_queue_.size() < max_queue_size; });
    if (!success_) {
      // failed in other workers
      return false;
    }
    task_queue_.push(std::move(task));
    task_cv_.notify_one();
  }
  // Mark the end of the input stream
  unique_lock<mutex> lock(task_mtx_);
  generate_finished_ = true;
  task_cv_.notify_all();
  return true;
}

bool FstdxCompressor::compress_imple(ZSTD_CCtx *cctx, const ZSTD_CDict *cdict,
                                     const CompressTask &task) {
  if (!success_) {
    // compression failed in other workers
    return false;
  }
  CompressResult result;
  result.index = task.index;
  size_t comp_dst_size = ZSTD_compressBound(task.src_data.size());
  result.dst_data.resize(comp_dst_size);
  size_t comp_size = ZSTD_compress_usingCDict(
      cctx, result.dst_data.data(), result.dst_data.size(),
      task.src_data.data(), task.src_data.size(), cdict);

  if (ZSTD_isError(comp_size)) {
    success_ = false;
    LOG_ERROR("Compress blocks failed: {}", ZSTD_getErrorName(comp_size));
    return false;
  }
  result.dst_data.resize(comp_size);
  {
    unique_lock<mutex> lock(res_mtx_);
    result_queue_.push(std::move(result));
    res_cv_.notify_one();
  }
  return true;
}

bool FstdxCompressor::compress_worker(const ZSTD_CDict *cdict) {
  ZSTD_CCtx *cctx = ZSTD_createCCtx();
  if (!cctx) { return false; }
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, ENABLE_CHECKSUM ? 1 : 0);
  while (true) {
    CompressTask task;
    {
      unique_lock<mutex> lock(task_mtx_);
      task_cv_.wait(lock,
                    [&] { return !task_queue_.empty() || generate_finished_; });

      if (task_queue_.empty() && generate_finished_) {
        break; // all tasks are done
      }
      task = std::move(task_queue_.front());
      task_queue_.pop();
      task_cv_.notify_one(); // notify next task
    }
    if (!compress_imple(cctx, cdict, task)) {
      ZSTD_freeCCtx(cctx);
      return false;
    }
  };
  ZSTD_freeCCtx(cctx);
  return true;
}

bool FstdxCompressor::block_writer(
    std::ostream &out,
    const std::vector<std::pair<size_t, size_t>> &block_record,
    DyBlockProgBars &dy_bars,
    std::vector<BlockIndex> &block_indexes, uint64_t &total_block_size) {
  block_indexes.clear();
  size_t expected_index = 0;
  // (Key: index, Value: Compressed Data)
  unordered_map<size_t, vector<char>> fallback_buffer;
  uint64_t block_offset = 0;

  auto refresh_bar = dy_bars.push_back(
      block_record.size(), "Compressing value blocks:", Color::white);

  while (true) {
    CompressResult result;
    {
      unique_lock<mutex> lock(res_mtx_);
      res_cv_.wait(lock, [&] {
        return !success_ || !result_queue_.empty() || compress_finished_;
      });

      if (!success_) {
        // compression failed in other workers
        return false;
      }
      if (result_queue_.empty() && compress_finished_ &&
          fallback_buffer.empty()) {
        break;
      }

      if (!result_queue_.empty()) {
        result = std::move(result_queue_.front());
        result_queue_.pop();
      }
    }

    if (!result.dst_data.empty() || result.index == expected_index) {
      fallback_buffer[result.index] = std::move(result.dst_data);
    }

    // write compressed blocks in order
    while (fallback_buffer.count(expected_index)) {
      if (!success_) {
        // compression failed in other workers
        return false;
      }
      auto &data = fallback_buffer[expected_index];

      uint64_t block_size = static_cast<uint64_t>(data.size());
      block_indexes.emplace_back(block_record[expected_index].second,
                                 block_offset, block_size,
                                 block_record[expected_index].first);
      block_offset += block_size;

      out.write(data.data(), block_size);
      total_block_size += block_size;
      refresh_bar(expected_index);

      fallback_buffer.erase(expected_index);
      expected_index++;
    }
  }
  return true;
}

bool FstdxCompressor::compress_texts_to_stream(
    std::ostream &out, const std::vector<std::string> &texts,
    DxJsonHeader &header, const char *dict_buffer, size_t dict_size,
    size_t block_size, int compression_level, ThreadPool &thread_pool,
    DyBlockProgBars &dy_bars) {
  ZSTD_CDict *cdict =
      ZSTD_createCDict(dict_buffer, dict_size, compression_level);
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
    if (block_buf_size >= block_size) {
      block_record.emplace_back(block_buf_size, i + 1);
      block_buf_size = 0;
    }
  }
  if (block_buf_size != 0) {
    block_record.emplace_back(block_buf_size, texts.size());
  }

  thread generator([&]() { block_generator(texts, block_record); });

  size_t worker_size = thread_pool.worker_num();
  vector<future<bool>> results;
  for (size_t i = 0; i < worker_size - 1; ++i) {
    results.emplace_back(
        thread_pool.enqueue([&]() { return compress_worker(cdict); }));
  }

  std::vector<BlockIndex> block_indexes;
  uint64_t total_block_size = 0;
  thread writer([&]() {
    block_writer(out, block_record, dy_bars, block_indexes, total_block_size);
  });

  generator.join();

  for (auto &res : results) {
    if (!res.get()) { success_ = false; }
  }

  // notify writer to finish
  {
    unique_lock<mutex> lock(res_mtx_);
    compress_finished_ = true;
    res_cv_.notify_all();
  }
  writer.join();

  if (!success_) {
    ZSTD_freeCDict(cdict);
    return false;
  }
  LOG_DEBUG("Compress done.");

  header["comp_blocks"]["compress_level"] = compression_level;
  header["comp_blocks"]["offset"] = 0;

  out.write(dict_buffer, dict_size);
  header["comp_dict"]["compress_level"] = 0;
  header["comp_dict"]["original_size"] = dict_size;
  header["comp_dict"]["offset"] =
      static_cast<size_t>(header["comp_blocks"]["offset"]) + total_block_size;

  bool comp_res = false;
  size_t comp_block_index_size = 0;
  {
    std::vector<char> comp_block_index_dst;
    comp_res =
        compress_to_buffer(reinterpret_cast<const char *>(block_indexes.data()),
                           block_indexes.size() * sizeof(BlockIndex),
                           comp_block_index_dst, compression_level);
    if (!comp_res) { return false; }
    out.write(comp_block_index_dst.data(), comp_block_index_dst.size());
    comp_block_index_size = comp_block_index_dst.size();
    header["block_indexes"]["compress_level"] = compression_level;
    header["block_indexes"]["compressed_size"] = comp_block_index_size;
    header["block_indexes"]["original_size"] =
        block_indexes.size() * sizeof(BlockIndex);
    header["block_indexes"]["offset"] =
        static_cast<size_t>(header["comp_dict"]["offset"]) + dict_size;
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

} // namespace fstd