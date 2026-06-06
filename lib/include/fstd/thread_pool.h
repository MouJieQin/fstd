#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

// 获取 CPU 逻辑核心数（兼容所有系统）
inline unsigned int get_cpu_core_count() {
  unsigned int core_num = std::thread::hardware_concurrency();
  // 容错：旧系统不支持时，默认返回 4 核心
  if (core_num == 0) { core_num = 4; }
  return core_num;
}

// 计算 最优线程数
enum class TaskType {
  CPU_INTENSIVE, // CPU 密集型
  IO_INTENSIVE   // IO 密集型
};

inline unsigned int get_optimal_thread_num(TaskType type) {
  unsigned int core_num = get_cpu_core_count();
  switch (type) {
  case TaskType::CPU_INTENSIVE:
    // CPU 密集：线程数 = 核心数（最大化利用率，无切换损耗）
    return core_num;
  case TaskType::IO_INTENSIVE:
    // IO 密集：核心数 × 2~5（通用最优系数，可根据业务调整）
    return core_num * 2;
  default: return core_num;
  }
}

class ThreadPool {
public:
  ThreadPool(size_t);
  template <class F, class... Args>
  auto enqueue(F &&f, Args &&...args)
      -> std::future<std::invoke_result_t<F, Args...>>;
  ~ThreadPool();

  size_t worker_num() const;

private:
  // 需要保留线程对象，让它们保持运行
  std::vector<std::thread> workers;
  // 任务队列
  std::queue<std::function<void()>> tasks;

  // 同步变量
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};

// 构造函数：启动指定数量的工作线程
inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
  for (size_t i = 0; i < threads; ++i)
    workers.emplace_back([this] {
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(this->queue_mutex);
          this->condition.wait(
              lock, [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty()) return;
          task = std::move(this->tasks.front());
          this->tasks.pop();
        }

        task();
      }
    });
}

inline size_t ThreadPool::worker_num() const { return workers.size(); }

// 添加任务到线程池
template <class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using return_type = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex);

    // 不允许在停止后加入任务
    if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks.emplace([task]() { (*task)(); });
  }
  condition.notify_one();
  return res;
}

// 析构函数
inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers)
    worker.join();
}

#endif