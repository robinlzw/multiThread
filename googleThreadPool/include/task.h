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

    State GetState() LOCKS_EXCLUDED(mutex_);  // 返回本Task当前状态

    // State must be 'NEW'.
    void SetWorkItem(const WorkItem& work_item) LOCKS_EXCLUDED(mutex_);  // 设置Task 执行的任务 （函数）

    // State must be 'NEW'. 'dependency' may be nullptr, in which case it is
    // assumed completed.
    // 给当前任务添加 依赖任务，如当前任务为b，添加依赖任务为a（a——>b: b.AddDependency(a))
    // 同时并把当前任务b，加入到依赖任务a的dependent_tasks_列表中，以便执行a后，对应更改b的状态）。
    void AddDependency(std::weak_ptr<Task> dependency) LOCKS_EXCLUDED(mutex_);
    void AddTaskInfo(const std::string& task_info, const int type=0) LOCKS_EXCLUDED(mutex_);
    std::string getTaskInfo() LOCKS_EXCLUDED(mutex_);

private:
    // Allowed in all states.
    // AddDependency 功能具体实现函数
    // 添加依赖本Task的Task，如b依赖a，则a-->b， a.AddDependentTask(b), 根据a的状态，改变b的状态
    // 如果a完成，则b的依赖-1；（并把当前任务b，加入到依赖任务a的 dependent_tasks_ 列表中，以便执行a后，对应更改b的状态）。
    void AddDependentTask(Task* dependent_task);

    // State must be 'DEPENDENCIES_COMPLETED' and becomes 'COMPLETED'.
    // 执行当前任务，比如当前任务为a，并依此更新依赖a的任务dependent_tasks_中所有任务状态，如依赖a的b。
    void Execute() LOCKS_EXCLUDED(mutex_);
    void  Remove()  LOCKS_EXCLUDED(mutex_);
    // State must be 'NEW' and becomes 'DISPATCHED' or 'DEPENDENCIES_COMPLETED'.
    // 当前任务进入线程待执行队列
    void SetThreadPool(ThreadPoolInterface* thread_pool) LOCKS_EXCLUDED(mutex_);

    // State must be 'NEW' or 'DISPATCHED'. If 'DISPATCHED', may become
    // 'DEPENDENCIES_COMPLETED'.
    // 当前任务的依赖任务完成时候，当前任务状态随之改变
    void OnDependenyCompleted();

    WorkItem work_item_ GUARDED_BY(mutex_);  // 任务具体执行过程
    ThreadPoolInterface* thread_pool_to_notify_ GUARDED_BY(mutex_) = nullptr;  // 执行当前任务的线程池
    State state_ GUARDED_BY(mutex_) = NEW;  // 初始化状态为 NEW
    unsigned int uncompleted_dependencies_ GUARDED_BY(mutex_) = 0;  // 当前任务依赖的任务的数量
    std::set<Task*> dependent_tasks_ GUARDED_BY(mutex_);  // 依赖当前任务的任务列表

    std::chrono::steady_clock::time_point add_time_;
    std::string info_ GUARDED_BY(mutex_);
    int         type_ GUARDED_BY(mutex_);//0:default; 1:create fast matcher; 2: local constrain; 3: global constrain 4: finish one node; 5: spa
    absl::Mutex mutex_;
};
#endif