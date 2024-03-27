#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EventLoop.h"
using namespace std;

// 定义子线程对应的结构体
class WorkerThread
{
public:
    WorkerThread(int index); // 构造函数，index表示当前线程是线程池中的第几个
    ~WorkerThread(); // 析构函数
    void run(); // 启动线程
    inline EventLoop* getEventLoop()
    {
        return m_evLoop;
    }

private:
    void running();

private:
    thread* m_thread;   // 保存线程的实例
    thread::id m_threadID; // 线程ID
    string m_name; // 线程的name
    mutex m_mutex;  // 互斥锁
    condition_variable m_cond; // 条件变量
    EventLoop* m_evLoop; // 反应堆模型
};

