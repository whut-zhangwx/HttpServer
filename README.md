## 基于Reactor模式的并发WebServer项目

### 项目简介

这是一个基于Reactor多反应堆模式（主从反应堆）的多线程（线程池）的Http的WebServer项目

### 运行项目

```shell
# clone项目
git clone https://github.com/whut-zhangwx/ReactorHttp-Cpp.git
# 切到项目目录
cd ./ReactorHttp-Cpp
# 编译整个项目（记得加上-l pthread参数）
g++ ./*.cpp -o ./server -lpthread
# 运行项目
./server
# port写在main中，默认为10000
# ip由INADDR_ANY自动获取
localhost:10000
```

### 项目骨架介绍

- main.cpp是项目的入口
  main.cpp中new了一个TcpServer对象，并调用它的run()方法用来启动整个项目。

- TcpServer对象所做的事情
  一个TcpServer对象将创建（使用new方法）一个线程池m_threadPool（指向ThreadPool类对象的指针）和一个主反应堆m_mainLoop（指向EventLoop类对象的指针），并运行线程池和主反应堆。

- 主反应堆的作用
  TcpServer::run()通过调用m_mainLoop->addTask()向主反应堆m_mainLoop中添加一个channel对象（添加到主反应堆的任务队列m_taskQ中），channel中封装了监听用的socket的fd（服务端的socket），它对应的事件类型FDEvent::ReadEvent（自定义的枚举类型），以及回调函数acceptConnection（事件FDEvent::ReadEvent类型对应的回调函数readFunc）。
  在主反应堆中，反应堆会通过processTask()方法将m_taskQ中的channel取出并注册添加到select的位数组（事件监听表）中，通过SelectDispatcher对象调用select方法对监听事件表中注册的事件进行监听。当监听到事件的发生后，会根据事件的fd找到m_channelMap中对应的channel对象，然后根据channel对象中的events（事件类型）来选择调用处理函数（实际就是TcpServer::acceptConnection）。

- ThreadPool线程池的作用
  在TcpServer对象的构造函数中会创建ThreadPool对象m_threadPool，在TcpServer::run()方法中，调用创建的线程池的run()方法m_threadPool->run()来启动整个线程池。在ThreadPool::run()中，会创建（new）m_threadNum个WorkerThread（工作线程）类对象，将它们的指针存储在m_workerThreads中（vecotr类型），并且调用WorkerThread::run()来启动这些工作线程。
- WorkerThread的作用
  一个WorkerThread对象就对应着一个子线程，它主要包含一个从反应堆成员，和子线程要运行的回调函数running()。通过调用run()方法来创建一个子线程运行running()。子线程中主要运行着从反应堆。

  在WorkerThread::run()中，我们会使用C++的std::thread方法来创建子线程，子线程执行WorkerThread::running()函数。在running()中，子线程会为WorkerThread创建一个从反应堆（EventLoop对象），并且启动这个从反应堆。
  创建完子线程的WorkerThread::run()会结束调用并返回（void），子线程继续运行WorkerThread::running()，直到使用某些方法结束其中从反应堆的while循环。

- 从反应堆的作用
  在主反应堆中，其通过SelectDispatcher对象调用select方法对在TcpServer中创建的监听socket（服务端socket）的文件描述符m_lfd进行监听（实际的m_lfd通过channel封装，先添加到主反应堆的任务队列m_taskQ中，再通过processTask注册添加到select的事件监听表中）。当select检测到m_lfd中有连接请求的时候，会根据m_lfd找到m_channelMap中的channel对象，调用其中的readFunc，即acceptConnection（因为m_lfd的events事件类型为FDEvent::ReadEvent）。
  在TcpServer::acceptConnection()中，会调用accept()函数和m_lfd对应的服务端的监听socket建立连接，返回客户端socket的文件描述符cfd。此时从线程池m_threadPool中取出一个WorkerThread对象，获取这个对象的从反应堆m_evLoop。将客户端socket的cfd以及这个子线程的从反应堆指针evLoop，封装成一个TcpConnection对象，之后在TcpConnection的构造函数中又会将cfd重新封装成一个channel，添加到这个个从反应堆evLoop的任务队列m_taskQ中。从反应堆一直在运行中（select监听 + while循环），它检测到m_taskQ不为空，将其中的channel取出添加cfd到select的事件监视表中。当select检测到cfd事件发生，会根据channel的中的事件类型调用相关的回调函数对cfd进行处理。这三个回调函数processRead、processWrite、destroy都是定义在TcpConnection类中的。

- TcpConnection类的作用
  TcpConnection类中定义了对客户端socket事件的三种回调函数processRead、processWrite、destroy，分别处理读、写、销毁的三种操作。
  对于processRead，它会调用HttpRequest类中的parseHttpRequest方法对http请求进行解析以及做出相应的response。（有限状态机）
