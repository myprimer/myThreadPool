#include "ThreadPool.hpp"

#include <iostream>
#include <chrono>
#include <mutex>

std::mutex coutMtx;  // protect std::cout

void task(int taskId) {
    {
        std::lock_guard<std::mutex> guard(coutMtx);
        std::cout << "task-id : " << taskId << " begin!" << std::endl;                    
    }

    // executing task for 2 second
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    {
        std::lock_guard<std::mutex> guard(coutMtx);
        std::cout << "task-id : " << taskId << " end!" << std::endl;        
    }
}


void monitor(ThreadPool &pool, int seconds) {
    for (int i = 1; i < seconds * 10; ++i)
    {
        {
            std::lock_guard<std::mutex> guard(coutMtx);
            std::cout << "thread num : " << pool.getBusyNum_() << std::endl;                    
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));            
    }
}

int main() {
    // max threads number is 5
    ThreadPool pool(20, 5);

    // monitoring threads number for 13 seconds    
    pool.commit(monitor, std::ref(pool), 13);
    
    // submit 100 tasks
    for (int taskId = 0; taskId < 100; ++taskId) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.commit(task, taskId);
    }
    
    return 0;
}