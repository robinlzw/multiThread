#ifndef CARTOGRAPHER_COMMON_THREAD_POOL_H_
#define CARTOGRAPHER_COMMON_THREAD_POOL_H_

#include <deque>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "task.h"


class Task;

class ThreadPoolInterface {
public:
    ThreadPoolInterface() {}
    virtual ~ThreadPoolInterface() {}
    virtual std::weak_ptr<Task> Schedule(std::unique_ptr<Task> task) = 0;

protected:
    void Execute(Task* task);
    void SetThreadPool(Task* task);

private:
    friend class Task;

    virtual void NotifyDependenciesCompleted(Task* task) = 0;
};

// A fixed number of threads working on tasks. Adding a task does not block.
// Tasks may be added whether or not their dependencies are completed.
// When all dependencies of a task are completed, it is queued up for execution
// in a background thread. The queue must be empty before calling the
// destructor. The thread pool will then wait for the currently executing work
// items to finish and then destroy the threads.
class ThreadPool : public ThreadPoolInterface {
public:
    explicit ThreadPool(int num_threads);  // 初始化一个线程数量固定的线程池
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // When the returned weak pointer is expired, 'task' has certainly completed,
    // so dependants no longer need to add it as a dependency.
    std::weak_ptr<Task> Schedule(std::unique_ptr<Task> task)
        LOCKS_EXCLUDED(mutex_) override;  // 添加想要ThreadPool执行的task，插入tasks_not_ready_,
    // 如果任务满足执行要求，直接插入task_queue_准备执行

private:
    void DoWork(); // 每个线程初始化时,执行DoWork()函数. 与线程绑定
    void DoWork(const int thread_id);
    void NotifyDependenciesCompleted(Task* task) LOCKS_EXCLUDED(mutex_) override;

    absl::Mutex mutex_;
    bool running_ GUARDED_BY(mutex_) = true;  // running_只是一个监视哨,只有线程池在running_状态时,才能往work_queue_加入函数.
    std::vector<std::thread> pool_ GUARDED_BY(mutex_);
    std::deque<std::shared_ptr<Task>> task_queue_ GUARDED_BY(mutex_);  // 准备执行的task
    absl::flat_hash_map<Task*, std::shared_ptr<Task>> tasks_not_ready_
        GUARDED_BY(mutex_);  // 未准备好的 task，task可能有依赖还未完成
};
#endif
