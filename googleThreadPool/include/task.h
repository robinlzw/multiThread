#ifndef CARTOGRAPHER_COMMON_TASK_H_
#define CARTOGRAPHER_COMMON_TASK_H_

#include <set>
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "thread_pool.h"


class ThreadPoolInterface;

class Task {
 public:
  friend class ThreadPoolInterface;

  using WorkItem = std::function<void()>;
  enum State { NEW, DISPATCHED, DEPENDENCIES_COMPLETED, RUNNING, COMPLETED };

  Task() = default;
  ~Task();

  State GetState() LOCKS_EXCLUDED(mutex_);

  // State must be 'NEW'.
  void SetWorkItem(const WorkItem& work_item) LOCKS_EXCLUDED(mutex_);

  // State must be 'NEW'. 'dependency' may be nullptr, in which case it is
  // assumed completed.
  void AddDependency(std::weak_ptr<Task> dependency) LOCKS_EXCLUDED(mutex_);
  void AddTaskInfo(const std::string& task_info, const int type=0) LOCKS_EXCLUDED(mutex_);
  std::string getTaskInfo() LOCKS_EXCLUDED(mutex_);

 private:
  // Allowed in all states.
  void AddDependentTask(Task* dependent_task);

  // State must be 'DEPENDENCIES_COMPLETED' and becomes 'COMPLETED'.
  void Execute() LOCKS_EXCLUDED(mutex_);
  void  Remove()  LOCKS_EXCLUDED(mutex_);
  // State must be 'NEW' and becomes 'DISPATCHED' or 'DEPENDENCIES_COMPLETED'.
  void SetThreadPool(ThreadPoolInterface* thread_pool) LOCKS_EXCLUDED(mutex_);

  // State must be 'NEW' or 'DISPATCHED'. If 'DISPATCHED', may become
  // 'DEPENDENCIES_COMPLETED'.
  void OnDependenyCompleted();

  WorkItem work_item_ GUARDED_BY(mutex_);
  ThreadPoolInterface* thread_pool_to_notify_ GUARDED_BY(mutex_) = nullptr;
  State state_ GUARDED_BY(mutex_) = NEW;
  unsigned int uncompleted_dependencies_ GUARDED_BY(mutex_) = 0;
  std::set<Task*> dependent_tasks_ GUARDED_BY(mutex_);

  std::chrono::steady_clock::time_point add_time_;
  std::string info_ GUARDED_BY(mutex_);
  int         type_ GUARDED_BY(mutex_);//0:default; 1:create fast matcher; 2: local constrain; 3: global constrain 4: finish one node; 5: spa
  absl::Mutex mutex_;
};
#endif