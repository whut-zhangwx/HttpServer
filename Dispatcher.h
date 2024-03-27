#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include <string>
using namespace std;

// Dispatcher类和EvenLoop类是互相包含的，所以这里需要对EventLoop进行声明
class EventLoop;

class Dispatcher
{
public:
    Dispatcher(EventLoop* evloop);
    // 虚函数，多态
    virtual ~Dispatcher();
    // 添加
    virtual int add();
    // 删除
    virtual int remove();
    // 修改
    virtual int modify();
    // 事件监测
    virtual int dispatch(int timeout = 2); // 单位: s
    inline void setChannel(Channel* channel)
    {
        m_channel = channel;
    }
    protected:
    string m_name = string();
    Channel* m_channel;
    EventLoop* m_evLoop;
};