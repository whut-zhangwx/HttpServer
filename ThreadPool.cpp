#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

// 构造函数
ThreadPool::ThreadPool(EventLoop* mainLoop, int count)
{
    m_index = 0; // 用于标识线程的index，当需要取出工作线程的时候，默认会取出m_workerThreads[m_index]，并将m_index++
    m_isStart = false;
    m_mainLoop = mainLoop; // 由TcpServer传入，mainLoop是主反应堆（其m_taskQ中存放着任务队列）
    m_threadNum = count; // 线程的数量
    m_workerThreads.clear(); // m_workerThreads是一个vector<WorkerThread*>类型，其中存放工作线程的指针（子线程对象的指针）
}

// 析构函数
ThreadPool::~ThreadPool()
{
    for (auto item : m_workerThreads)
    {
        delete item;
    }
}

void ThreadPool::run()
{
    assert(!m_isStart); // 如果m_isStart==true表示线程池已经启动，则assert
    // 判断反应堆的线程id是否等于当前线程的id
    if (m_mainLoop->getThreadID() != this_thread::get_id()) // std::this_thread
    {
        exit(0);
    }
    m_isStart = true; // 标识线程池已经启动
    if (m_threadNum > 0)
    {
        for (int i = 0; i < m_threadNum; ++i)
        {
            WorkerThread* subThread = new WorkerThread(i); // new一个新的工作线程对象，参数i标识线程对象的序号
            subThread->run(); // run()中真正创建子线程，并执行子线程的工作函数
            /*WorkerThread对象中会创建一个子线程，子线程中会new一个新的EventLoop（从反应堆）对象，
            从反应堆running时会通过processTaskQ()从自己的m_taskQ任务队列中取出channel对象，将其封装的fd注册添加到监听事件表中。
            然后，从反应堆会通过SelectDispatcher对象，对监听事件表中注册的事件进行监听。如果监听到事件的发生，则调用
            从反应堆的eventActive()方法，根据事件的类型对事件进行dispatch（分发处理）。*/

            m_workerThreads.push_back(subThread); // 将线程对象的指针放进vector中
        }
    }
}

// 在channel的回调函数中被调用TcpServer::acceptConnection
EventLoop* ThreadPool::takeWorkerEventLoop()
{
    assert(m_isStart);
    if (m_mainLoop->getThreadID() != this_thread::get_id())
    {
        exit(0);
    }
    // 从线程池中找一个子线程, 然后取出里边的反应堆实例
    EventLoop* evLoop = m_mainLoop;
    if (m_threadNum > 0)
    {
        // 按顺序从线程池的m_workerThreads（存储子线程对象的指针的vector）中取出一个子线程对象的指针WorkerThread
        // 从子线程对象中获得它的从反应堆实例的指针getEventLoop()
        evLoop = m_workerThreads[m_index]->getEventLoop();
        // m_index++
        m_index = ++m_index % m_threadNum;
    }
    // 返回拿到的子线程的反应堆对象的指针
    return evLoop;
}
