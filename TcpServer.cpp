#include "TcpServer.h"
#include <arpa/inet.h>
#include "TcpConnection.h"
#include <stdio.h>
#include <stdlib.h>
#include "Log.h"

// TcpServer构造函数
TcpServer::TcpServer(unsigned short port, int threadNum)
{
    m_port = port;
    m_mainLoop = new EventLoop; // 实例化EventLoop，主反应堆，它的m_taskQ中存放着任务队列
    // EventLoop类，即Reactor，负责对事件进行反应，即监听和分发事件，事件类型包含连接事件、读写事件等
    // EventLoop类中声明了一个select方法（实际是封装了select方法的SelectDispatcher类对象）；

    m_threadNum = threadNum; // 线程数
    m_threadPool = new ThreadPool(m_mainLoop, threadNum); // 实例化线程池，传入的是主反应堆对象的指针
    // 线程池中new了threadNum个子线程对象，它们的指针保存在一个vector中；每个子线程又包含一个(从)反应堆对象

    setListen(); // 调用setListen()
}

void TcpServer::setListen()
{
    // 1. 创建监听的fd
    m_lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_lfd == -1)
    {
        perror("socket");
        return;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1)
    {
        perror("setsockopt");
        return;
    }
    // 3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(m_lfd, (struct sockaddr*)&addr, sizeof addr);
    if (ret == -1)
    {
        perror("bind");
        return ;
    }
    // 4. 设置监听
    ret = listen(m_lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return ;
    }
    // 5. 输出监听端口的ip:port
    struct sockaddr_in listenAddr;
    socklen_t listenAddrLen = sizeof(listenAddr);
    ret = getsockname(m_lfd, (struct sockaddr *)&listenAddr, &listenAddrLen);
    if(ret == -1)
    {
        printf("getsockname error\n");
        exit(0);
    }
    printf("listening address = %s:%d\n", inet_ntoa(listenAddr.sin_addr), ntohs(listenAddr.sin_port));

}

int TcpServer::acceptConnection(void* arg)
{
    TcpServer* server = static_cast<TcpServer*>(arg); // 将void*类型转换成TcpServer*类型
    // 和客户端建立连接
    int cfd = accept(server->m_lfd, NULL, NULL);

    // 输出客户端的ip:port
    struct sockaddr_in connectedAddr;
    socklen_t connectedAddrLen = sizeof(connectedAddr);
    int ret = getsockname(cfd, (struct sockaddr *)&connectedAddr, &connectedAddrLen);
    if(ret == -1)
    {
        printf("getsockname error\n");
        exit(0);
    }
    printf("connected address = %s:%d\n", inet_ntoa(connectedAddr.sin_addr), ntohs(connectedAddr.sin_port));

    // 从线程池中取出一个子线程的从反应堆实例, 去处理这个cfd（按顺序取出反应堆）
    EventLoop* evLoop = server->m_threadPool->takeWorkerEventLoop();
    // 将cfd放到 TcpConnection中处理，传入反应堆对象的指针evLoop
    /*这里将连接的客户端的socket的fd，以及一个子线程的从反应堆指针，封装成一个TcpConnection
    之后在TcpConnection中又会将任务重新封装成一个channel，添加到子线程从反应堆的任务队列m_taskQ中*/
    new TcpConnection(cfd, evLoop);
    return 0;
}

void TcpServer::run()
{
    Debug("服务器程序已经启动了...");
    // 启动线程池
    m_threadPool->run();
    // 初始化一个channel实例
    /*Channel::handleFunc readFunc = accepConnection, Channel::handleFunc writeFunc=nullptr, Channel::handleFunc destroyFunc=nullptr*/
    // m_lfd是setListen()中创建的监听用的socket的文件描述符，其对应的事件为FDEvent::ReadEvent
    Channel* channel = new Channel(m_lfd, FDEvent::ReadEvent, acceptConnection, nullptr, nullptr, this);

    // 添加channel到主反应堆的任务队列m_taskQ中
    // channel中封装了监听用的socket的fd，它对应的事件类型FDEvent::ReadEvent，以及回调函数acceptConnection
    /* 在反应堆中，反应堆会通过processTask()方法将m_taskQ中的channel取出并注册添加到select的位数组（事件监听表）中，
     通过SelectDispatcher对象调用select方法对事件进行监听，当监听到事件的发生后，会根据事件的fd找到m_channelMap中对应的channel对象
     根据channel对象中的events（事件类型）来选择调用处理函数，如果事件类型是ReadEvent则调用readFunc（这里添加的channel就是这样）*/
    m_mainLoop->addTask(channel, ElemType::ADD);
    // 启动反应堆模型
    m_mainLoop->run();
}
