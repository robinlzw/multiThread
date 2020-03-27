#include <set>
#include "absl/synchronization/mutex.h"
#include "glog/logging.h"
#include "thread_pool.h"


#define LOCKS_EXCLUDED
#define GUARDED_BY


class Task {
 public:
  friend class ThreadPoolInterface;

  using WorkItem = std::function<void()>;
  enum State { NEW, DISPATCHED, DEPENDENCIES_COMPLETED, RUNNING, COMPLETED };

  Task() = default;
  ~Task();
  
  State GetState() LOCKS_EXCLUDED(mutex_);     //返回本Task当前状态 
  void SetWorkItem(const WorkItem& work_item); //设置Task 执行的任务 （函数）
  // 给当前任务添加 依赖任务，如当前任务为b，添加依赖任务为a（a——>b: b.AddDependency(a))
  // 同时并把当前任务b，加入到依赖任务a的dependent_tasks_列表中，以便执行a后，对应更改b的状态）。
  void AddDependency(std::weak_ptr<Task> dependency) LOCKS_EXCLUDED(mutex_);

 private:

  // AddDependency功能具体实现函数
  // 添加依赖本Task的Task，如b依赖a，则a-->b， a.AddDependentTask(b), 根据a的状态，改变b的状态
  // 如果a完成，则b的依赖-1；（并把当前任务b，加入到依赖任务a的dependent_tasks_列表中，以便执行a后，对应更改b的状态）。
  void AddDependentTask(Task* dependent_task);

  // 执行当前任务，比如当前任务为a，并依此更新依赖a的任务dependent_tasks_中所有任务状态，如依赖a的b。
  void Execute() LOCKS_EXCLUDED(mutex_);

  // 当前任务进入线程待执行队列
  void SetThreadPool(ThreadPoolInterface* thread_pool) LOCKS_EXCLUDED(mutex_);

  // 当前任务的依赖任务完成时候，当前任务状态随之改变
  void OnDependenyCompleted();

  WorkItem work_item_ ;// 任务具体执行过程
  ThreadPoolInterface* thread_pool_to_notify_ = nullptr;// 执行当前任务的线程池
  State state_ GUARDED_BY(mutex_) = NEW; // 初始化状态为 NEW
  unsigned int uncompleted_dependencies_ GUARDED_BY(mutex_) = 0;  //当前任务依赖的任务的数量
  std::set<Task*> dependent_tasks_ GUARDED_BY(mutex_);// 依赖当前任务的任务列表
  absl::Mutex mutex_;
};
