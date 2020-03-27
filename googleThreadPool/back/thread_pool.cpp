#include "thread_pool.h"

#ifndef WIN32
#include <unistd.h>
#endif
#include <algorithm>
#include <chrono>
#include <numeric>

#include "absl/memory/memory.h"
#include "task.h"
#include "glog/logging.h"

namespace cartographer {
namespace common {

void ThreadPoolInterface::Execute(Task* task) {
    task->Execute();
  //LOG(INFO)<<"Execute finish: ";//<<task->getTaskInfo() ;
}

void ThreadPoolInterface::SetThreadPool(Task* task) {
  task->SetThreadPool(this);
}

ThreadPool::ThreadPool(int num_threads) {
  absl::MutexLock locker(&mutex_);
  for (int i = 0; i != num_threads; ++i) {
    pool_.emplace_back([this]() { static int id = 0; ThreadPool::DoWork(id++);});
  }
}

ThreadPool::~ThreadPool() {
  LOG(INFO)<<" ~ThreadPool "<<running_<< "  task_queue_: "<<task_queue_.size();
  {
    absl::MutexLock locker(&mutex_);
    CHECK(running_);
    running_ = false;
  }
  for (std::thread& thread : pool_) {
    thread.join();
    LOG(INFO)<<" join "<<&thread<<" "<<running_;
  }
}

void ThreadPool::NotifyDependenciesCompleted(Task* task) {
  absl::MutexLock locker(&mutex_);
  auto it = tasks_not_ready_.find(task);
  CHECK(it != tasks_not_ready_.end());
  //LOG(INFO)<<"NotifyDependenciesCompleted task_queue_: "<<task_queue_.size()<<" ready: "<<tasks_not_ready_.size();// <<" |task_info: "<<task->getTaskInfo();
  task_queue_.push_back(it->second);
  tasks_not_ready_.erase(it);
  //LOG(INFO)<<"==>==>: "<<task_queue_.size()<<" ready: "<< tasks_not_ready_.size() ;
}

std::weak_ptr<Task> ThreadPool::Schedule(std::unique_ptr<Task> task) {
  std::shared_ptr<Task> shared_task;
  {
    absl::MutexLock locker(&mutex_);
    auto insert_result =
        tasks_not_ready_.insert(std::make_pair(task.get(), std::move(task)));
    CHECK(insert_result.second) << "Schedule called twice";
    shared_task = insert_result.first->second;
    //LOG(INFO)<<"ThreadPool Schedule: "<<task_queue_.size()<<" ready:"<<tasks_not_ready_.size();// <<" |task_info: "<<task->getTaskInfo();
  }
  SetThreadPool(shared_task.get());
  return shared_task;
}

void ThreadPool::DoWork() {
#ifdef __linux__
  // This changes the per-thread nice level of the current thread on Linux. We
  // do this so that the background work done by the thread pool is not taking
  // away CPU resources from more important foreground threads.
  CHECK_NE(nice(10), -1);
#endif
  const auto predicate = [this]() EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !task_queue_.empty() || !running_;
  };
  for (;;) {
    std::shared_ptr<Task> task;
    {
      absl::MutexLock locker(&mutex_);
      mutex_.Await(absl::Condition(&predicate));
      if (!task_queue_.empty()) {
        task = std::move(task_queue_.front());
        task_queue_.pop_front();
        //LOG(INFO)<<" task_queue_: "<<task_queue_.size();
      } else if (!running_) {
        return;
      }
    }
    CHECK(task);
    CHECK_EQ(task->GetState(), common::Task::DEPENDENCIES_COMPLETED);
    Execute(task.get());
  }
}


void ThreadPool::DoWork(const int thread_id) {
#ifdef __linux__
  // This changes the per-thread nice level of the current thread on Linux. We
  // do this so that the background work done by the thread pool is not taking
  // away CPU resources from more important foreground threads.
  CHECK_NE(nice(10), -1);
#endif
  const auto predicate = [this]() EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !task_queue_.empty() || !running_;
  };
  for (;;) {
    std::shared_ptr<Task> task;
    {
      absl::MutexLock locker(&mutex_);
      mutex_.Await(absl::Condition(&predicate));
/*      if (!running_) {
          LOG(WARNING)<<"DoWork:  running_"<<thread_id;
          return;
      }
      else*/
      if (!task_queue_.empty()) {
          task = std::move(task_queue_.front());
          //if(!task->getTaskInfo().empty())
          static int last_read_size = 0;
          if((task_queue_.size()>4 ||tasks_not_ready_.size()>8 ) && (tasks_not_ready_.size()- last_read_size)%10 == 0)
          {
              last_read_size = tasks_not_ready_.size();
              LOG(WARNING)<<"thread_id: "<<thread_id<<" task_queue_: "<<task_queue_.size()<<" ready_queue:"<<tasks_not_ready_.size() ;//<<" | "<<task->getTaskInfo();
          }

          task_queue_.pop_front();
          //LOG(INFO)<<"==>==>:task_queue_:  "<<task_queue_.size()<<" ready_queue: "<< tasks_not_ready_.size() ;
      }
      else if (!running_) {
          LOG(WARNING)<<"DoWork:  running_"<<thread_id;
          return;
      }
    }
    CHECK(task);
    CHECK_EQ(task->GetState(), common::Task::DEPENDENCIES_COMPLETED);
    Execute(task.get());
  }
}

