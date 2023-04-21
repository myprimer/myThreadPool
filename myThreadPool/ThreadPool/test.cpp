#include "ThreadPool.hpp"

#include <iostream>
#include <chrono>
#include <mutex>

std::mutex Mtx;  // protect std::cout

// 执行 task 任务需要至少 2s
void task(int taskId) {
    {
        std::lock_guard<std::mutex> guard(Mtx);
        std::cout << "   task-id : " << taskId << " begin!" << std::endl;                    
    }

    // executing task for 2 second
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    {
        std::lock_guard<std::mutex> guard(Mtx);
        std::cout << "   task-id : " << taskId << " end!" << std::endl;        
    }
}

// 每隔 200ms 监视一下当前线程池中在忙的线程数，一共监视 12 * 10 * 200ms = 24000ms = 24s
void monitor(ThreadPool &pool, int seconds) {
    for (int i = 1; i < seconds * 10; ++i)
    {
        {
            std::lock_guard<std::mutex> guard(Mtx);
            std::cout << "busy thread num : " << pool.getBusyNum_() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));            
    }
}

int main() {

    ThreadPool pool(5, 10);

    // monitoring threads number for 12 seconds    
    pool.commit(monitor, std::ref(pool), 12);
    
    // submit 100 tasks，每隔 100ms 添加一个task任务，一共添加 100个 
    for (int taskId = 1; taskId <= 30; ++taskId) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.commit(task, taskId);
    }
    // 休眠 10s
    std::this_thread::sleep_for(std::chrono::seconds(10));

    for (int taskId = 31; taskId <= 50; ++taskId) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.commit(task, taskId);
    }
    
    return 0;
}
