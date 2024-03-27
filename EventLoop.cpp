#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SelectDispatcher.h"
#include "PollDispatcher.h"
#include "EpollDispatcher.h"

// 构造函数，string()表示空字符串，调用下面的有参构造函数，初始化threadName为空字符串
EventLoop::EventLoop() : EventLoop(string())
{
}

EventLoop::EventLoop(const string threadName)
{
    m_isQuit = true; // 默认没有启动（退出状态）
    m_threadID = this_thread::get_id(); // 获取当前线程的线程id
    // string()返回空字符串，下面判断threadName是否是个空字符串
    m_threadName = threadName == string() ? "MainThread" : threadName;
    // m_dispatcher是个父类指针，之后可以基于指向不同的子类EpollDispatcher|PollDispatcher|SelectDispathcer实现多态的效果
    m_dispatcher = new SelectDispatcher(this); // 这里指向SelectDispatcher，传入当前反应堆对象的this指针
    // map
    m_channelMap.clear(); // 初始化map
    // 创建一个双向通信的管道，管道两端的文件描述符存放在m_socketPair[0],m_socketPair[1]
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
#if 0
    // 指定规则: evLoop->socketPair[0] 发送数据, evLoop->socketPair[1] 接收数据
    // readLocalMessage是个静态成员函数
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
        readLocalMessage, nullptr, nullptr, this);
#else
    // 绑定 - bind，让可调用对象可以像函数一样被使用
    auto obj = bind(&EventLoop::readMessage, this);
    // 指定规则: evLoop->socketPair[0] 发送数据, evLoop->socketPair[1] 接收数据
    Channel* channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
        obj, nullptr, nullptr, this);
#endif
    // channel 添加到任务队列
    addTask(channel, ElemType::ADD);
}

// 析构函数
EventLoop::~EventLoop()
{
}

int EventLoop::run()
{
    m_isQuit = false;
    // 比较线程ID是否正常，正常情况下相同
    if (m_threadID != this_thread::get_id())
    {
        return -1;
    }
    // 循环进行事件处理
    while (!m_isQuit)
    {
        // 调用dispatch()对已经注册的事件进行监听，这里的dispatch()是子类SelectDispatcher中的方法，所以使用的是select()方法进行监听
        // 对于发生的事件，dispatch()中会更具事件的类型，通过传入的当前反应堆对象的this指针，调用其eventActive()方法对其进行具体处理
        // 即所谓的分配（dispatch）
        // 如果超时，则不会调用eventActive()对任务进行处理
        m_dispatcher->dispatch(); // 超时时长默认为2s，这个调用是个多态，实际调用的dispatch方法在子类中

        // processTaskQ()会从任务队列m_taskQ中取出任务结点node，从node中拿出channel
        // 根据channel对应的任务类型，分别调用add()|remove()|modify()，将channel中的m_fd添加（例如）到m_dispatch的监听事件表中
        // 例如调用add()，则将channel对象中的m_fd注册到SelectDispatcher对象的监听表m_readSet或m_writeSet中
        processTaskQ();
    }
    return 0;
}

// 根据事件event的类型，对fd进行处理
// 在m_dispatcher->dispatch()中被调用
int EventLoop::eventActive(int fd, int event)
{
    if (fd < 0)
    {
        return -1;
    }
    // 取出channel
    Channel* channel = m_channelMap[fd]; // 获取fd对应的channel
    assert(channel->getSocket() == fd);
    // &是位运算，&&是逻辑运算
    // readCallback和writeCallback是在 TcpServer::run() 中创建channel时传入的
    if (event & (int)FDEvent::ReadEvent && channel->readCallback) // 处理读事件
    {
        // channel->getArg()返回一个const void*类型的arg，这个arg实际是传入的this指针（TcpServer或者TcpConnection对象）
        // const_cast<void*>用于移除arg的const限定符
        // 在readCallback()函数内部，void*类型的arg又会被重新转换成TcpServer*类型的指针，指向它对应的TcpServer对象
        channel->readCallback(const_cast<void*>(channel->getArg()));
    }
    if (event & (int)FDEvent::WriteEvent && channel->writeCallback) // 处理写事件
    {
        channel->writeCallback(const_cast<void*>(channel->getArg()));
    }
    return 0;
}

// 在TcpServer::run()中被调用
// addTask()方法是将任务添加到任务队列m_taskQ中
// 一个任务就是一个channel以及它对应的事件类型(ElemType::ADD|DELETE|MODIFY)，使用ChannelElement结构体将其封装成一个node
// channel对象封装了任务的文件描述符m_fd（例如监听的socket），fd的事件类型(FDEvent::ReadEvent|WriteEvent|TimeOut)，以及它对应的回调函数（可调用对象类型）
// 在调用eventActive()具体处理任务的时候，会根据Channel::m_events的值，来选择调用三种回调函数 
int EventLoop::addTask(Channel* channel, ElemType type)
{
    // 加锁, 保护共享资源
    m_mutex.lock();
    // 创建新节点，封装channel和type
    ChannelElement* node = new ChannelElement;
    node->channel = channel;
    node->type = type;
    // 添加结点到任务队列中
    m_taskQ.push(node);
    m_mutex.unlock();
    // 处理节点
    /*
    * 细节:
    *   1. 对于链表节点的添加: 可能是当前线程也可能是其他线程(主线程)
    *       1). 修改fd的事件, 当前子线程发起, 当前子线程处理
    *       2). 添加新的fd, 添加任务节点的操作是由主线程发起的
    *   2. 不能让主线程处理任务队列, 需要由当前的子线程取处理
    */
    if (m_threadID == this_thread::get_id())
    {
        // 当前子线程(基于子线程的角度分析)
        processTaskQ();
    }
    else
    {
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1. 子线程在工作 2. 子线程被阻塞了:select, poll, epoll
        taskWakeup();
    }
    return 0;
}

// 处理任务队列m_taskQ中的任务
// processTaskQ()会从任务队列m_taskQ中取出任务结点node，从node中拿出channel
// 根据channel对应的任务类型，分别调用add()|remove()|modify()，将channel中的m_fd添加（例如）到m_dispatch的监听事件表中
// 例如调用add()，则将channel对象中的m_fd注册到SelectDispatcher对象的监听表m_readSet或m_writeSet中
int EventLoop::processTaskQ()
{
    // 取出头结点
    while (!m_taskQ.empty())
    {
        m_mutex.lock();
        ChannelElement* node = m_taskQ.front(); // 取出一个任务结点
        m_taskQ.pop();  // 删除节点
        m_mutex.unlock(); // 解锁
        Channel* channel = node->channel; // 取出channel
        // 根据type判断需要处理的操作
        if (node->type == ElemType::ADD)
        {
            // 添加
            add(channel);
        }
        else if (node->type == ElemType::DELETE)
        {
            // 删除
            remove(channel);
        }
        else if (node->type == ElemType::MODIFY)
        {
            // 修改
            modify(channel);
        }
        delete node;
    }
    return 0;
}

int EventLoop::add(Channel* channel)
{
    int fd = channel->getSocket();
    // 找到fd对应的数组元素位置, 并存储
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        m_channelMap.insert(make_pair(fd, channel));
        // 先设置m_dispatcher的m_channel = channel
        m_dispatcher->setChannel(channel); // setChannel的声明在父类头文件中，将channel指针传递给成员m_channel
        // 然后调用m_dispatcher->add(), 将m_channel的fd，根据对应的事件类型，注册到m_readSet或m_writeSet中
        int ret = m_dispatcher->add();
        // 添加成功ret==0，失败ret==-1
        return ret;
    }
    return -1;
}

int EventLoop::remove(Channel* channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->remove();
    return ret;
}

int EventLoop::modify(Channel* channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end())
    {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->modify();
    return ret;
}

int EventLoop::readLocalMessage(void* arg)
{
    EventLoop* evLoop = static_cast<EventLoop*>(arg);
    char buf[256];
    read(evLoop->m_socketPair[1], buf, sizeof(buf));
    return 0;
}

void EventLoop::taskWakeup()
{
    const char* msg = "我是要成为海贼王的男人!!!";
    write(m_socketPair[0], msg, strlen(msg));
}

int EventLoop::freeChannel(Channel* channel)
{
    // 删除 channel 和 fd 的对应关系
    auto it = m_channelMap.find(channel->getSocket());
    if (it != m_channelMap.end())
    {
        m_channelMap.erase(it);
        close(channel->getSocket());
        delete channel;
    }
    return 0;
}

int EventLoop::readMessage()
{
    char buf[256];
    read(m_socketPair[1], buf, sizeof(buf));
    return 0;
}
