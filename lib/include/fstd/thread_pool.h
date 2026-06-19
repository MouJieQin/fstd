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

inline unsigned int get_cpu_core_count() {
  unsigned int core_num = std::thread::hardware_concurrency();
  if (core_num == 0) { core_num = 4; }
  return core_num;
}

enum class TaskType { CPU_INTENSIVE, IO_INTENSIVE };

inline unsigned int get_optimal_thread_num(TaskType type) {
  unsigned int core_num = get_cpu_core_count();
  switch (type) {
  case TaskType::CPU_INTENSIVE: return core_num;
  case TaskType::IO_INTENSIVE: return core_num * 2;
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

  size_t task_num() const;

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  // Marked mutable to allow locking within the const worker_num() function
  mutable std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};

inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
  // 1. Lock the pool during initialization to prevent premature thread
  // execution races
  std::unique_lock<std::mutex> lock(queue_mutex);

  // 2. Critical: Reserve memory to prevent vector reallocation while threads
  // are spawning
  workers.reserve(threads);

  for (size_t i = 0; i < threads; ++i) {
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
}

inline size_t ThreadPool::worker_num() const {
  // 3. Lock the mutex to ensure safe reading of the vector size across threads
  std::unique_lock<std::mutex> lock(queue_mutex);
  return workers.size();
}

inline size_t ThreadPool::task_num() const {
  std::unique_lock<std::mutex> lock(queue_mutex);
  return tasks.size();
}

template <class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using return_type = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex);

    if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks.emplace([task]() { (*task)(); });
  }
  condition.notify_one();
  return res;
}

inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) {
    // 4. Defensive check to ensure the thread is joinable before calling join()
    if (worker.joinable()) { worker.join(); }
  }
}

#endif
