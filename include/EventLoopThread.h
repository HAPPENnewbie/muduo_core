#pragma once

#include <functional>
#include <mutex>      // 互斥锁，保证线程安全
#include <condition_variable>  // 条件变量，用于线程间同步
#include <string>

#include "noncopyable.h"
#include "Thread.h"        // 封装线程操作的自定义类（如创建、启动线程）

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    // 定义线程初始化回调函数类型：参数是EventLoop*，无返回值
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();  // 开始循环

private:
    // 线程的入口函数：在新线程中创建并运行EventLoop
    void threadFunc();

    EventLoop *loop_;     // 指向当前线程的EventLoop对象
    bool exiting_;          // 标识线程是否要退出
    Thread thread_;             // 封装的线程对象（自定义Thread类）
    std::mutex mutex_;              // 互斥锁：保护loop_的访问，实现线程同步
    std::condition_variable cond_;  // 条件变量：配合mutex_实现线程间等待/通知
    ThreadInitCallback callback_;   // 线程初始化回调函数
};