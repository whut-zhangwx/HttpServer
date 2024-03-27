#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stack>
using namespace std;

std::mutex Mutex;
std::condition_variable Condv;
std::stack<int> Stack;

void Consume() {
  std::unique_lock<std::mutex> locker(Mutex); // 上锁，对Stack进行上锁
  // 如果stack为空，则进入等待
  while(Stack.empty()) {
    Condv.wait(locker); // 通过locker进行解锁
  }
  cout << "Consume: " << Stack.size() << endl;
}

void Produce() {
  Mutex.lock(); // 加锁
    Stack.push(1);
  Mutex.unlock(); // 解锁
  Condv.notify_one(); // 通知Consume停止阻塞
  std::cout << "Produce: " << Stack.size() << endl;
}

int main() {
  std::thread t(Consume); // 子线程调用Consume()
  t.detach();
  Produce(); // 主线程调用Produce()
  return 0;
}