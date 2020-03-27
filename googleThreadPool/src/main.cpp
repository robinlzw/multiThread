#include "task.h"
#include "thread_pool.h"
#include <iostream>


void first(int id) {
    std::cout << "hello from " << id << ", function\n";
}

void aga(int id, int par) {
    std::cout << "hello from " << id << ", function with parameter " << par <<'\n';
}


int main(int argc, char** argv) { 
    // HOME_PATH   
    std::string home = "/home/lizw/myDev_ws/cpp_projects/202003/multiThread/googleThreadPool/build/apps/log/";  //要先创建此目录,否则运行报错.
    //glog init   
    google::InitGoogleLogging(argv[0]);    
    FLAGS_log_dir = home;
    // std::string info_log = home + "master_info_";   
    // google::SetLogDestination(google::INFO, info_log.c_str());   
    // std::string warning_log = home + "master_warning_";   
    // google::SetLogDestination(google::WARNING, warning_log.c_str());   
    // std::string error_log = home + "master_error_";   
    // google::SetLogDestination(google::ERROR, error_log.c_str());   
   
    ThreadPool googleThreadPool(2);
    auto test_task = absl::make_unique<Task>();
    test_task->SetWorkItem([=]() LOCKS_EXCLUDED(test_task->mutex_) {
        first(1);
    });
    test_task->AddTaskInfo("task_name", 2);    
    auto test_task_handle = googleThreadPool.Schedule(std::move(test_task));
    
    
    return 0;
}