#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include <string>
#include <sys/epoll.h>
using namespace std;

class EpollDispatcher : public Dispatcher // Dispatcher类的子类
{
public:
    EpollDispatcher(EventLoop* evloop);
    ~EpollDispatcher();
    // 添加
    int add() override; // override是C++11的关键字，表示其父类对应的方法是个虚函数，子类需要重写这个方法
    // 删除
    int remove() override;
    // 修改
    int modify() override;
    // 事件监测
    int dispatch(int timeout = 2) override; // 单位: s

private:
    int epollCtl(int op);

private:
    // epoll的相关操作
    int m_epfd;
    struct epoll_event* m_events;
    const int m_maxNode = 520;
};