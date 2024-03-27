#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

class TcpServer
{
public:
    TcpServer(unsigned short port, int threadNum); // 构造函数
    // 初始化监听
    void setListen();
    // 启动服务器
    void run();
    static int acceptConnection(void* arg);

private:
    int m_threadNum;
    EventLoop* m_mainLoop; // 主反应堆
    ThreadPool* m_threadPool; // 线程池
    int m_lfd;
    unsigned short m_port;
};

