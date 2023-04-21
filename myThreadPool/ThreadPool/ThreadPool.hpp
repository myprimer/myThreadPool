#ifndef __THREADPOOL__
#define __THREADPOOL__

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <future>

class ThreadPool {
public:
    using Task = std::function<void()>;
    using UniqueLock = std::unique_lock<std::mutex>;
    
    // 构造函数
    explicit ThreadPool(int minThreadNum = std::thread::hardware_concurrency(), int maxThreadNum = 16) : is_exit_(false), busyNum_(0) {
        minThreadNum_ = minThreadNum;
        maxThreadNum_ = maxThreadNum;
        addThread(minThreadNum_);
        managerThread_ = std::move(std::thread(&ThreadPool::manager, this));
    }

    // 析构函数
    ~ThreadPool() {
        is_exit_ = true;

        // 回收管理线程
        if (managerThread_.joinable()) {
            managerThread_.join();
        }

        // 唤醒所有阻塞的消费者线程，空闲的线程解除阻塞后判断到线程池关闭后就会调用线程退出函数自杀
        if (workerThreads_.size() > 0) {
            cv_.notify_all();
        }

        for (auto& th : workerThreads_) {
            if (th.joinable()) {
                th.join(); // 等待任务结束
            }
        }
        UniqueLock lock(mtx_);
        workerThreads_.clear(); // 清空线程数组
    }

    // 禁止拷贝和移动
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator= (ThreadPool&&) = delete;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator= (const ThreadPool&) = delete;

    int getBusyNum_() {
        return busyNum_;
    }

    // 提交任务
    // commit()是一个模板函数，class... Args是可变模版参数。
    template <class F, class... Args>
    // 这里函数类型的定义用到了尾返回类型推导。
    auto commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        if (is_exit_) { // stoped
            throw std::runtime_error("commit on ThreadPool is stopped.");
        }

        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        // std::packaged_task<decltype(f(args...))()>将前面std::function方法声明的特殊函数包装func传入作为std::packaged_task的实例化参数。
        // 智能指针方便对该std::packaged_task对象进行管理。
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        // std::function将task_ptr指向的std::packaged_task对象取出并包装为void函数。
        
        {
            UniqueLock lock(mtx_);
            taskQueue_.emplace([task_ptr](){
                (*task_ptr)();
            });
        }
        cv_.notify_one(); // 唤醒一个线程执行
        return task_ptr->get_future();
    }

    void addThread(int size) {
        for (; workerThreads_.size() < maxThreadNum_ && size > 0; --size) {
            workerThreads_.emplace_back([this](){
                std::cout << "√ work thread id : " << std::this_thread::get_id() << "  is created..." << std::endl;   
                
                while (true) {
                    Task task;
                    {
                        UniqueLock lock(mtx_);
                        cv_.wait(lock, [this](){        // 线程池没有退出且任务队列为空，阻塞
                            return is_exit_ || !taskQueue_.empty();
                        }); 
                        // 线程池退出且任务队列为空，管理者线程就唤醒阻塞的线程让其自杀
                        if (is_exit_ && taskQueue_.empty()) {
                            lock.unlock();
                            std::cout << "work thread id : " << std::this_thread::get_id() << "  finished..." << std::endl;
                            return; // return 退出线程函数，一个线程就会销毁
                        }
                        task = std::move(taskQueue_.front()); // 从队列取一个task
                        taskQueue_.pop();
                    }
                    ++busyNum_; // 忙线程数量加1
                    task(); // 执行任务
                    --busyNum_; // 忙线程数量减1
                }
            });
        }
    }

// 管理者线程任务函数：每隔 1s 动态的创建/销毁线程
    void manager() {
        const int NUMBER = 2;
        while (!is_exit_) {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // 每隔1s检测一次
            int threads_num = 0;
            int taskQueue_num = 0;
            {
                UniqueLock lock(mtx_);
                threads_num = workerThreads_.size();
                taskQueue_num = taskQueue_.size();
            }

            // 添加线程 , 最多一次添加 NUMBER 个线程，如果一次添加后超过了 最大维护线程数量，那么就一次就添加一个
            // 任务数 > 线程数 && 线程数 < 最大维护线程数
            if (taskQueue_num > threads_num && threads_num < maxThreadNum_) {
                int number = 1;
                if (threads_num + NUMBER < maxThreadNum_) number = NUMBER;
                addThread(number);
            }

            // 销毁线程 , 一次销毁 NUMBER 个线程
            // 忙的线程* 2 < 线程数 && 线程数 > 最小维护线程数
            if (busyNum_ * 2 < threads_num && threads_num > minThreadNum_) {
                // 让阻塞的工作线程自杀
                std::cout << "× destory a thread..." << std::endl;
                cv_.notify_one();
            }
        }
    }

private:
    std::mutex mtx_; // 互斥锁
    std::vector<std::thread> workerThreads_; // 工作线程
    // std::unordered_map<std::thread::id, std::thread> workerThreads_;     // 工作线程
    unsigned int minThreadNum_; // 最小维护线程数量
	unsigned int maxThreadNum_; // 最大维护线程数量
    std::condition_variable cv_; // 条件变量
    std::queue<Task> taskQueue_; // 任务队列
    std::atomic<bool> is_exit_; // 线程池是否退出
    std::atomic<int>  busyNum_; // 当前运行任务的数量
    std::thread managerThread_; // 管理者线程

};


#endif
