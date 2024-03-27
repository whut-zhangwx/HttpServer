#pragma once
#include "Dispatcher.h"
#include "Channel.h"
#include <thread>
#include <queue>
#include <map>
#include <mutex>
using namespace std;

// 处理该节点中的channel的方式（指定了强类型枚举，类型为char；默认的int类型占4个字节，char只占1个字节，可以节省空间）
enum class ElemType:char{ADD, DELETE, MODIFY};
// 定义任务队列的节点
struct ChannelElement
{
    ElemType type; // 如何处理该节点中的channel，type==ADD,DEKETE,MODIFY
    Channel* channel;
};

// Dispatcher类和EvenLoop类是互相包含的，所以这里需要对Dispatcher进行声明
class Dispatcher;

class EventLoop
{
public:
    EventLoop();
    EventLoop(const string threadName);
    ~EventLoop();
    // 启动反应堆模型
    int run();
    // 处理被激活的文件描述符，event为FDEvent枚举类型，为TimeOut|ReadEvent|WriteEvent
    int eventActive(int fd, int event);
    // 添加任务channel到任务队列m_Qtask
    int addTask(struct Channel* channel, ElemType type);
    // 处理任务队列中的任务
    int processTaskQ();
    // processTaskQ()根据任务的类型ADD, DELETE, MODIFY分别调用下面的三个函数处理任务
    int add(Channel* channel);
    int remove(Channel* channel);
    int modify(Channel* channel);
    // 释放channel
    int freeChannel(Channel* channel);
    int readMessage(); //待定
    // 返回线程ID
    inline thread::id getThreadID()
    {
        return m_threadID;
    }
    inline string getThreadName()
    {
        return m_threadName;
    }
    static int readLocalMessage(void* arg);

private:
    void taskWakeup();

private:
    bool m_isQuit; // 用于标记当前的EventLoop是不是正在running，如果是则m_isQuit==false，否则为true
    // Dispatcher*是个父类指针，它通过指向不同子类的实例 EpollDispather, PollDispatcher, SelectDispathcher，从而实现多态
    Dispatcher* m_dispatcher;
    // 任务队列
    queue<ChannelElement*> m_taskQ; // <--任务队列，使用queue实现
    // map
    map<int, Channel*> m_channelMap; // 用于存储文件描述符，和文件描述符封装后对应的Channel类对象
    // 线程id, name, mutex
    thread::id m_threadID; // 线程id，类型是thread::id
    string m_threadName;
    mutex m_mutex; // 互斥锁
    int m_socketPair[2]; // 存储本地通信的fd 通过socketpair 初始化
};
