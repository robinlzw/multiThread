#include "task.h"
#include "iostream"

Task::~Task() {
  // TODO(gaschler): Relax some checks after testing.
  if (state_ != NEW && state_ != COMPLETED) {
    std::cout<<"--> remove task:"<< info_<<std::endl;
//    //Remove();
//    absl::MutexLock locker(&mutex_);
//    state_ = COMPLETED;
//    for (Task* dependent_task : dependent_tasks_) {
//      if(dependent_task == nullptr)
//        continue;
//      dependent_task->OnDependenyCompleted();
//    }
    //LOG(WARNING) << "Delete Task between dispatch and completion.";
  }
}

void Task::AddTaskInfo(const std::string& task_info, const int type)
{
    absl::MutexLock locker(&mutex_);
    info_ = task_info;
    type_ = type;
    add_time_ = std::chrono::steady_clock::now();
    //LOG(INFO) <<"--> ADD task:"<< task_info;
    //std::cout<<"--> ADD task:"<< task_info<<std::endl;
}

std::string Task::getTaskInfo()
{
    absl::MutexLock locker(&mutex_);
    return info_;
}

Task::State Task::GetState() {
  absl::MutexLock locker(&mutex_);
  return state_;
}

void Task::SetWorkItem(const WorkItem& work_item) {
  absl::MutexLock locker(&mutex_);
  CHECK_EQ(state_, NEW);
  work_item_ = work_item;
}

void Task::AddDependency(std::weak_ptr<Task> dependency) {
  std::shared_ptr<Task> shared_dependency;
  {
    absl::MutexLock locker(&mutex_);
    CHECK_EQ(state_, NEW);
    if ((shared_dependency = dependency.lock())) {
      ++uncompleted_dependencies_;
    }
  }
  if (shared_dependency) {
    shared_dependency->AddDependentTask(this);
  }
}

void Task::SetThreadPool(ThreadPoolInterface* thread_pool) {
  absl::MutexLock locker(&mutex_);
  CHECK_EQ(state_, NEW);
  state_ = DISPATCHED;
  thread_pool_to_notify_ = ;
  if (uncompleted_dependencies_ == 0) {
    state_ = DEPENDENCIES_COMPLETED;
    CHECK(thread_pool_to_notify_);
    thread_pool_to_notify_->NotifyDependenciesCompleted(this);
  }
}

void Task::AddDependentTask(Task* dependent_task) {
  absl::MutexLock locker(&mutex_);
  if (state_ == COMPLETED) {
    dependent_task->OnDependenyCompleted();
    return;
  }
  bool inserted = dependent_tasks_.insert(dependent_task).second;
  CHECK(inserted) << "Given dependency is already a dependency.";
}

void Task::OnDependenyCompleted() {
  absl::MutexLock locker(&mutex_);
  CHECK(state_ == NEW || state_ == DISPATCHED);
  --uncompleted_dependencies_;
  if (uncompleted_dependencies_ == 0 && state_ == DISPATCHED) {
    state_ = DEPENDENCIES_COMPLETED;
    CHECK(thread_pool_to_notify_);
    thread_pool_to_notify_->NotifyDependenciesCompleted(this);
  }
}

void Task::Execute() {
  {
    absl::MutexLock locker(&mutex_);
    CHECK_EQ(state_, DEPENDENCIES_COMPLETED);
    state_ = RUNNING;
  }

    // Execute the work item.
  if (work_item_) {
      std::chrono::steady_clock::time_point start_calc_time = std::chrono::steady_clock::now();
      work_item_();
      if(!info_.empty())
      {
          double time_cost_sec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start_calc_time).count();
          double stay_cost_sec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - add_time_).count();
          //LOG(INFO)<<"==> RUN Task: type "<<type_<<" time_cost: "<<time_cost_sec<<"  info: "<<info_<<" |stay_time "<<stay_cost_sec;
          //std::cout<<"==> RUN task: "<<info_<<std::endl;;
      }
  }

  absl::MutexLock locker(&mutex_);
  state_ = COMPLETED;
  for (Task* dependent_task : dependent_tasks_) {
    dependent_task->OnDependenyCompleted();
  }
}


void Task::Remove() {
  state_ = RUNNING;
    // Execute the work item.
  if (work_item_) {
      //work_item_();
      if(!info_.empty())
      {
          //LOG(INFO)<<"==>SKIP RUN task: "<<info_;
          std::cout<<"==>SKIP RUN task: "<<info_<<std::endl;
      }
  }

  absl::MutexLock locker(&mutex_);
  state_ = COMPLETED;
  for (Task* dependent_task : dependent_tasks_) {
    dependent_task->OnDependenyCompleted();
  }
}

