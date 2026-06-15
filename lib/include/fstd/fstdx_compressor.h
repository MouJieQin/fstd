#pragma once
#include <string>
#include <vector>

#include <fstd/common.h>
#include <fstd/thread_pool.h>
#include <indicators/block_progress_bar.hpp>
#include <zstd.h>

constexpr bool ENABLE_CHECKSUM = true;
namespace fstd {

class FstdxCompressor {
public:
  FstdxCompressor() = default;
  ~FstdxCompressor() = default;

  bool compress_texts_to_stream(
      std::ostream &out, const std::vector<std::string> &texts,
      DxJsonHeader &header, size_t dict_size, size_t block_size,
      int compression_level, ThreadPool &thread_pool,
      DyProgBars<indicators::BlockProgressBar> &dy_bars);

private:
  int random_int(int max);

  int train_zstd_dictionary(const std::vector<std::string> &texts,
                            char *dict_buffer, size_t dict_size);

  bool compress_texts_to_stream(
      std::ostream &out, const std::vector<std::string> &texts,
      DxJsonHeader &header, const char *dict_buffer, size_t dict_size,
      size_t block_size, int compression_level, ThreadPool &thread_pool,
      DyProgBars<indicators::BlockProgressBar> &dy_bars);

  void init();

  bool
  block_generator(const std::vector<std::string> &texts,
                  const std::vector<std::pair<size_t, size_t>> &block_record);

  bool compress_imple(ZSTD_CCtx *cctx, const ZSTD_CDict *cdict,
                      const CompressTask &task);

  bool compress_worker(const ZSTD_CDict *cdict);

  bool block_writer(std::ostream &out,
                    const std::vector<std::pair<size_t, size_t>> &block_record,
                    DyProgBars<indicators::BlockProgressBar> &dy_bars,
                    std::vector<BlockIndex> &block_indexes,
                    uint64_t &total_block_size);

private:
  std::queue<CompressTask> task_queue_;
  std::queue<CompressResult> result_queue_;
  std::mutex task_mtx_;
  std::mutex res_mtx_;
  std::condition_variable task_cv_;
  std::condition_variable res_cv_;

  std::atomic<bool> success_{true};
  std::atomic<bool> generate_finished_{false};
  std::atomic<bool> compress_finished_{false};
};

} // namespace fstd