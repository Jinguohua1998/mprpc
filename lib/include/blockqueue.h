#pragma once

#include <queue>
#include <thread>
#include <mutex>    // pthread_mutex_t
#include <condition_variable>   // pthread_condition_t

// 异步写日志的日志队列
template<typename T>
class BlockQueue{
public:
    // 多个worker线程都会写日志queue
    void Push(const T& data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);  
        // 相当于用智能指针封装了互斥锁，lock构造时，加锁，析构时，解锁
        m_queue.push(data);
    }

    // 一个线程读日志queue，写日志文件
    T Pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())     // 防止假唤醒，所以这里用while，消费者线程再判断一次
        {
            // 日志队列为空，线程进入wait状态
            m_condvariable.wait(lock);
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }
    
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};