/*********************************************************
*
*  Copyright (C) 2014 by Vitaliy Vitsentiy
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*********************************************************/


#ifndef __ctpl_stl_thread_pool_H__
#define __ctpl_stl_thread_pool_H__

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <queue>
#include <iostream>


// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type


namespace ctpl {

    namespace detail {
        template <typename T>
        class Queue {
        public:
            bool push(T const & value) {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->q.push(value);
                return true;
            }
            // deletes the retrieved element, do not use for non integral types
            bool pop(T & v) {
                std::unique_lock<std::mutex> lock(this->mutex);
                if (this->q.empty())
                    return false;
                v = this->q.front();
                this->q.pop();
                return true;
            }
            bool empty() {
                std::unique_lock<std::mutex> lock(this->mutex);
                return this->q.empty();
            }
        private:
            std::queue<T> q;
            std::mutex mutex;
        };
    }

    class thread_pool {

    public:

        thread_pool() { this->init(); }
        thread_pool(int nThreads) { this->init(); this->resize(nThreads); }

        // the destructor waits for all the functions in the queue to be finished
        ~thread_pool() {
            this->stop(true);
        }

        // get the number of running threads in the pool
        int size() {     return static_cast<int>(this->threads.size()); }

        // number of idle threads
        int n_idle() { return this->nWaiting; }
        std::thread & get_thread(int i) { return *this->threads[i]; }

        // change the number of threads in the pool
        // should be called from one thread, otherwise be careful to not interleave, also with this->stop()
        // nThreads must be >= 0
        // Resize函数很危险，应尽量少调用，若必须调用，则应当在创建线程池的那个线程内调用，而不要在其他线程中调用。
        void resize(int nThreads) {

            // 如果两个变量 is_stop_ 、 is_done_ 都不为真，表明线程池仍在使用，可以更改线程池内工作线程的数量，否则没必要对一个停用的线程池更改工作线程的数量。
            if (!this->isStop && !this->isDone) {
                int oldNThreads = static_cast<int>(this->threads.size());

                // 若新线程数 n_threads 大于当前的工作线程数 old_n_threads ，则将工作线程数组 threads_ 和线程标志数组 flags_ 的尺寸修改为新数目，
                // 同时使用for循环调用 SetThread(i) 函数逐个重新创建工作线程；
                if (oldNThreads <= nThreads) {  // if the number of threads is increased
                    this->threads.resize(nThreads);
                    this->flags.resize(nThreads);

                    for (int i = oldNThreads; i < nThreads; ++i) {
                        this->flags[i] = std::make_shared<std::atomic<bool>>(false);
                        this->set_thread(i);
                    }
                }
                
                // 若新线程数 n_threads 小于当前的工作线程数 old_n_threads ，则将先完成 old_n_threads - n_threads 个线程正在执行的任务，
                // 之后将工作线程数组 threads_ 和线程标志数组 flags_ 的尺寸修改为新数目。
                else {  // the number of threads is decreased
                    for (int i = oldNThreads - 1; i >= nThreads; --i) {
                        *this->flags[i] = true;  // this thread will finish
                        this->threads[i]->detach();
                    }
                    {
                        // stop the detached threads that were waiting
                        std::unique_lock<std::mutex> lock(this->mutex);
                        this->cv.notify_all();
                    }
                    this->threads.resize(nThreads);  // safe to delete because the threads are detached
                    this->flags.resize(nThreads);  // safe to delete because the threads have copies of shared_ptr of the flags, not originals
                }
            }
        }

        // empty the queue
        void clear_queue() {
            std::function<void(int id)> * _f;
            while (this->q.pop(_f))
                delete _f; // empty the queue
        }

        // pops a functional wrapper to the original function
        std::function<void(int)> pop() {
            std::function<void(int id)> * _f = nullptr;
            this->q.pop(_f);

            // 函数里使用一个小花招，即创建一个智能指针 func，当超出该对象的作用域时，
            // 就会在其析构函数中调用delete运算符释放内存。如果任务队列q_中存储的是智能指针，就不必使用这种小花招来释放内存了。
            std::unique_ptr<std::function<void(int id)>> func(_f); // at return, delete the function even if an exception occurred
            std::function<void(int)> f;
            if (_f)
                f = *_f;
            return f;
        }

        // wait for all computing threads to finish and stop all threads
        // may be called asynchronously to not pause the calling thread while waiting
        // if isWait == true, all the functions in the queue are run, otherwise the queue is cleared without running the functions
        // 停止线程池工作，若不允许等待，则直接停止当前正在执行的工作线程，同时清空任务队列；若允许等待，则等待当前正在执行的工作线程完成
        void stop(bool isWait = false) {
            if (!isWait) {
                if (this->isStop)
                    return;
                this->isStop = true;
                for (int i = 0, n = this->size(); i < n; ++i) {
                    *this->flags[i] = true;  // command the threads to stop
                }
                this->clear_queue();  // empty the queue
            }
            else {
                if (this->isDone || this->isStop)
                    return;
                this->isDone = true;  // give the waiting threads a command to finish
            }
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                this->cv.notify_all();  // stop all waiting threads
            }
            for (int i = 0; i < static_cast<int>(this->threads.size()); ++i) {  // wait for the computing threads to finish
                    if (this->threads[i]->joinable())
                        this->threads[i]->join();
            }
            // if there were no threads in the pool but some functors in the queue, the functors are not deleted by the threads
            // therefore delete them here
            this->clear_queue();
            this->threads.clear();
            this->flags.clear();
        }

        template<typename F, typename... Rest>
        auto push(F && f, Rest&&... rest) ->std::future<decltype(f(0, rest...))> {
            // 因为任务函数f的声明各式各样(参数个数/返回值类型)……因此不能将其直接存储到任务队列 q，
            // 于是先利用 std::bind 函数将其包装为一个异步操作任务 std::packaged_task<decltype(f(0, rest...))(int)> 对象 pck，
            //（接受一个整型参数，返回值类型为(f(0, rest...)函数的返回值类型）。
            // std::placeholders::_1 表示通过 std::bind 函数绑定后得到的异步任务对象接受的第一个参数是自由参数
            auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
                std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
            );
            
            // 利用Lambda表达式将 pck 包装为一个 std::function<void(int id)> 对象，这样就可以存储到任务队列 q 中了                
            // 直接使用new运算符创建裸指针_f，后面还需想办法释放指针内存
            auto _f = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
            });
            this->q.push(_f);
            std::unique_lock<std::mutex> lock(this->mutex);  // 对互斥量加锁

            // 使用条件变量 std::condition_variable 对象 cv_.notify_one() 函数通知各个线程任务队列已经发生了改变，
            // 让空闲线程赶紧从任务队列中拉取新任务执行；最后通过 pck->get_future() 返回一个 std::future 对象，以便调用者能从中取出函数执行完毕后的返回值。
            this->cv.notify_one();
            
            // Push 函数的返回值为一个 std::future 对象，std::future 对象内存储的数据类型由f(0, rest...)函数的返回值类型确定;
            // decltype(f(0, rest...))的作用就是获取 (f(0, rest...) 函数的返回值类型。
            // std::future 提供一种异步操作结果的访问机制，从字面意思来理解，它表示未来，名字非常贴切，
            // 因为一个异步操作的结果不可能马上获取，只能在未来某个时候得到。
            return pck->get_future();
        }

        // run the user's function that excepts argument int - id of the running thread. returned value is templatized
        // operator returns std::future, where the user can get the result and rethrow the catched exceptins
        template<typename F>
        auto push(F && f) ->std::future<decltype(f(0))> {            
            auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
            auto _f = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
            });
            this->q.push(_f);
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
            return pck->get_future();
        }


    private:

        // deleted
        thread_pool(const thread_pool &);// = delete;
        thread_pool(thread_pool &&);// = delete;
        thread_pool & operator=(const thread_pool &);// = delete;
        thread_pool & operator=(thread_pool &&);// = delete;

        // SetThread 函数的作用重新创建指定序号i的工作线程
        void set_thread(int i) {
            std::cout << "------------------------- set_thread(" << i << ") -------------------------" << std::endl;
            std::cout << "flag = " << *this->flags[i] << std::endl;

            // 使用 flags[i] 来初始化标志变量 flag
            std::shared_ptr<std::atomic<bool>> flag(this->flags[i]); // a copy of the shared ptr to the flag
            
            // 创建一个 Lambda 表达式变量 f --> 将之作为第i个线程的任务，该任务保存有创建时的{1.this(主线程创建的线程池对象); 2.i(任务id); 3.flag(标记变量)}
            auto f = [this, i, flag/* a copy of the shared ptr to the flag */]() {
                std::atomic<bool> & _flag = *flag;
                std::function<void(int id)> * _f;
                bool isPop = this->q.pop(_f);
                while (true) {
                    while (isPop) {  // if there is anything in the queue
                        // 如果任务队列 q 中存储的是智能指针，就不必使用这种小花招来释放内存了。
                        std::unique_ptr<std::function<void(int id)>> func(_f); // at return, delete the function even if an exception occurred
                        (*_f)(i);  // 执行任务函数
                        std::cout << "------------------------- (*_f)(" << i << ") -------------------------" << std::endl;
                        if (_flag)
                            return;  // the thread is wanted to stop, return even if the queue is not empty yet
                        else
                            isPop = this->q.pop(_f);
                    }
                    // the queue is empty here, wait for the next command
                    // 这里必须使用 std::unique_lock ，因为后面条件变量 cv 等待期间，需要解锁。
                    std::unique_lock<std::mutex> lock(this->mutex);
                    ++this->nWaiting;

                    // 等待任务队列传来的新任务
                    // 那么Lambda表达式变量f何时启动呢？当任务队列 q.pop(_f) 的返回值为 true 时，表明从任务队列 q 中取到了一个新任务，
                    // 于是调用 (*_f)(i); 执行之，如果当前任务队列没有任务，则使用下面的 this->cv.wait(...) 来等待新任务的到来，
                    // 在新任务到来之前，当前工作线程处于休眠状态。
                    this->cv.wait(lock, [this, &_f, &isPop, &_flag](){ isPop = this->q.pop(_f); return isPop || this->isDone || _flag; });
                    --this->nWaiting;
                    if (!isPop)
                        return;  // if the queue is empty and this->isDone == true or *flag then return
                }
            };

            // 使用Lambda表达式变量f作为工作线程的任务函数，创建序号为i的工作线程   
            this->threads[i].reset(new std::thread(f)); // compiler may not support std::make_unique()
        }

        void init() { this->nWaiting = 0; this->isStop = false; this->isDone = false; }

        std::vector<std::unique_ptr<std::thread>> threads;
        std::vector<std::shared_ptr<std::atomic<bool>>> flags;
        detail::Queue<std::function<void(int id)> *> q;
        std::atomic<bool> isDone;
        std::atomic<bool> isStop;
        std::atomic<int> nWaiting;  // how many threads are waiting

        std::mutex mutex;
        std::condition_variable cv;
    };
}

#endif // __ctpl_stl_thread_pool_H__
