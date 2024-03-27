#include "Dispatcher.h"
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include "SelectDispatcher.h"

// evloop是EventLoop调用SelectDispatcher(this)时传入的this指针
// evloop指针被用于初始化父类的成员m_evLoop
// Selectdispatcher继承了父类的成员m_evLoop，所以Selectdispatcher中可以通过m_evLoop指针调用反应堆中的方法
// 虽然SelectDispatcher对象是属于evloop指向的反应堆对象的
SelectDispatcher::SelectDispatcher(EventLoop* evloop) :Dispatcher(evloop)
{
    FD_ZERO(&m_readSet);
    FD_ZERO(&m_writeSet);
    m_name = "Select";
}

SelectDispatcher::~SelectDispatcher()
{
}

int SelectDispatcher::add()
{
    if (m_channel->getSocket() >= m_maxSize)
    {
        return -1;
    }
    // 通过setFdSet()将m_channel的文件描述符注册到m_readSet或m_writeSet中
    // m_channel的文件描述符是可以是服务端的socket（listen），对应的事件类型是FDEvent::ReadEvent（枚举类型）
    // 也可以是客户端的socket（connect），对应的事件类型是FDEvent::WriteEvent（枚举类型）
    setFdSet();
    return 0;
}

int SelectDispatcher::remove()
{
    clearFdSet();
    // 通过 channel 释放对应的 TcpConnection 资源
    m_channel->destroyCallback(const_cast<void*>(m_channel->getArg()));

    return 0;
}

int SelectDispatcher::modify()
{
    setFdSet();
    clearFdSet();
    return 0;
}

int SelectDispatcher::dispatch(int timeout) // timeout默认2s（默认参数的声明在父类头文件中）
{
    struct timeval val;
    val.tv_sec = timeout;
    val.tv_usec = 0;
    fd_set rdtmp = m_readSet; // 每次调用都要重置
    fd_set wrtmp = m_writeSet; // 每次调用都要重置
    // 调用select监测发生的事件，返回事件的数目（select是阻塞函数）
    // m_maxSize是最大的被监听事件的文件描述符
    // 将发生待读取事件的文件描述符放入rdtmp中
    // 将发生可写入事件的文件描述符放入wrtmp中
    // select超时时返回0，发生错误时返回-1，正常情况下返回发生事件的文件描述符的数量
    int count = select(m_maxSize, &rdtmp, &wrtmp, NULL, &val);
    if (count == -1)
    {
        perror("select");
        exit(0);
    }
    // 虽然我们知道发生事件的数量，但是我们并不知道发生事件的具体的文件描述符
    // 例如fd3和fd8对应事件发生，那么位数组rdtmp或者wrtmp的第3和第8位的值被置1；
    // 我们知道有2个事件发生，但不能直接知道具体的fd，需要遍历rdtmp和wrtmp来找到被置1的位置
    for (int i = 0; i < m_maxSize; ++i)
    {
        // m_evLoop是在父类中声明的，EventLoop* m_evLoop;
        if (FD_ISSET(i, &rdtmp)) // 判断i（文件描述符）是否在rdtmp中
        {
            m_evLoop->eventActive(i, (int)FDEvent::ReadEvent);
        }

        if (FD_ISSET(i, &wrtmp)) // 判断i（文件描述符）是否在wrtmp中
        {
            m_evLoop->eventActive(i, (int)FDEvent::WriteEvent);
        }
    }
    return 0;
}

void SelectDispatcher::setFdSet()
{
    // 通过调用FD_SET宏定义向m_readSet和m_writeSet位数组中注册(添加)需要监视的事件的文件描述符（将其fd值对应位置的值改为1）
    // 其中的m_channel是父类中的成员，在EventLoop通过调用setChannel(channel)方法将channel指针传递给成员m_channel
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent)
    {
        FD_SET(m_channel->getSocket(), &m_readSet);
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent)
    {
        FD_SET(m_channel->getSocket(), &m_writeSet);
    }
}

void SelectDispatcher::clearFdSet()
{
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent)
    {
        FD_CLR(m_channel->getSocket(), &m_readSet);
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent)
    {
        FD_CLR(m_channel->getSocket(), &m_writeSet);
    }
}
