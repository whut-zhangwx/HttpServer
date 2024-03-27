#include "WorkerThread.h"
#include <stdio.h>

// 子线程的回调函数，即子线程要执行的函数
void WorkerThread::running()
{
    m_mutex.lock();
    m_evLoop = new EventLoop(m_name); // new一个新的反应堆实例，该反应堆为从反应堆，其属于子线程
    m_mutex.unlock();
    m_cond.notify_one(); // 唤醒阻塞在这个条件变量m_cond上的某1个线程
    m_evLoop->run(); // 启动从反应堆
}

/*
WorkerThread类对象在TreadPool::run()中被创建，一个WorkerThread类对象就对应着一个子线程
子线程在WorkerThread::run()中通过调用c++的std::thread类方法创建
*/
WorkerThread::WorkerThread(int index)
{
    m_evLoop = nullptr; // WorkerThread对象所属的从反应堆对象的指针
    m_thread = nullptr; // m_thread是个std:thread*类型的指针，它指向一个thread对象
    m_threadID = thread::id(); // C++11中的ID不是一个整型，不能直接用0对其进行初始化，需要调用thread::id()对其进行初始化（返回一个无效的ID）
    m_name =  "SubThread-" + to_string(index);
}

WorkerThread::~WorkerThread()
{
    if (m_thread != nullptr)
    {
        delete m_thread;
    }
}

void WorkerThread::run()
{
    // 通过C++11的线程类thread创建子线程
    // &WorkerThread::running是线程任务函数的地址，this是当前WorkerThread对象的指针，它作为参数被传入
    m_thread = new thread(&WorkerThread::running, this);

    unique_lock<mutex> locker(m_mutex); // 加锁，对m_evLoop进行加锁
    /* 在WorkerThread::running()中，子线程会调用 m_evLoop = new EventLoop(m_name)，创建一个从反应堆对象，返回该对象的指针赋值给m_evLoop。
    这里使用while(m_evLoop == nullptr)阻塞主线程, 让主线程等待子线程完成从反应堆的创建（m_evLoop指针的赋值）*/
    while (m_evLoop == nullptr)
    {   
        // 主线程使用条件变量的wait()进入阻塞状态，直到子线程完成m_evLoop的创建，并调用m_cond.notify_one()通知主线程，主线程可以继续运行
        // 注意上面使用的是while而不是if，因为有可能wait()会被其操作系统唤醒（虚假唤醒），而此时m_evLoop还没有被赋与指针，所以需要用while
        m_cond.wait(locker);
    }
    /*为什么条件变量m_cond需要和互斥锁m_mutex(locker)配合使用？
    因为不对m_evLoop加锁可能会出现这种情况，就是主线程还没有运行到m_cond.wait()时，子线程已经对m_evLoop完成了赋值，并进行了notify_one()，
    但是由于主线程还没有开始wait()，所以它错过了这次notify(唤醒丢失)，当它运行到wait()时会继续阻塞，这样会出问题。
    所以需要对m_evLoop进行加锁，让子线程阻塞在对m_evLoop的修改上，等待主线程运行到wait(locker)，并使用locker进行解锁后，子线程才可以完成
    对m_evLoop的修改，进而调用notify_one()通知主线程继续运行*/

    // 函数结束前会自动解锁
}
