#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}


// 启动 EventLoopThread 封装的子线程,让主线程安全等待子线程创建好 EventLoop 对象, 返回子线程的 EventLoop 指针，供主线程后续使用；
EventLoop *EventLoopThread::startLoop()
{
    // 1. 启动子线程：真正创建操作系统级别的线程，并执行threadFunc()
    thread_.start(); // 启用底层线程Thread类对象thread_中通过start()创建的线程
    // 2. 初始化局部变量，用于接收子线程的EventLoop指针
    EventLoop *loop = nullptr; 
    // 3. 加锁 + 条件变量等待：保证主线程拿到有效的EventLoop指针
    {
        // 3.1 加锁：保护共享变量loop_的访问
        std::unique_lock<std::mutex> lock(mutex_);
        // 3.2 条件变量等待：直到loop_不为空（子线程创建好EventLoop）
        cond_.wait(lock, [this](){return loop_ != nullptr;});
        // 3.3 条件满足，获取子线程的EventLoop指针
        loop = loop_;
    }
    // 4. 返回有效的EventLoop指针给调用者
    return loop;
}

// EventLoopThread 类中线程的入口函数，负责创建并运行独立的 EventLoop，同时完成线程间的同步。是在单独的新线程里运行的
void EventLoopThread::threadFunc()
{
    // 1. 创建子线程专属的EventLoop对象（one loop per thread核心）
    EventLoop loop; 
    // 2. 执行自定义初始化回调（可选扩展）
    if (callback_) 
    {
        callback_(&loop);
    }
    // 3. 线程同步：通知主线程EventLoop已创建完成,花括号限定了锁的周期
    {
        std::unique_lock<std::mutex> lock(mutex_);  
        loop_ = &loop;
        cond_.notify_one();
    }
    // 4. 启动EventLoop的事件循环，也就是开启了底层的Poller的poll()
    loop.loop();    
    // 5. 事件循环退出后，清理共享指针（线程收尾）
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;  //事件循环退出后，清理 EventLoop 指针，完成线程收尾
}