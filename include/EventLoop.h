#pragma once

#include <functional>   // 用于std::function（回调函数封装）
#include <vector>       // 存储Channel列表
#include <atomic>       // 原子变量（线程安全的布尔值）
#include <memory>       // 智能指针（管理Poller/Channel生命周期）
#include <mutex>        // 互斥锁（保护共享数据）

#include "noncopyable.h"    // 禁用拷贝构造/赋值（单例/线程绑定类常用）
#include "Timestamp.h"      // 时间戳类（记录事件发生时间）
// #include "CurrentThread.h"  // 当前线程工具类（获取线程ID）

// 前置声明：避免包含完整头文件，减少编译依赖
// 这两个类是 EventLoop 的核心依赖，但此处仅需声明（无需定义），减少编译耦合。
class Channel;
class Poller;

// 事件循环类 主要包含了两个大模块 Channel Poller(epoll的抽象)
class EventLoop : noncopuable   // 继承noncopyable，禁止拷贝/赋值
{
public:
    using Functor = std::function<void()>;   // 回调函数类型别名（无参无返回值）

    // 构造/析构函数
    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    //  返回 Poller（如 epoll）返回就绪事件的时间戳，用于监控事件处理延迟、日志记录等
    Timestamp pollReturnTime() const { return pollRetureTime_; }

    // 在当前loop的线程中执行回调（如果当前线程是loop所属线程，直接执行；否则放入队列）
    void runInLoop(Functor cb);
    // 把回调放入队列，唤醒loop所在线程执行（跨线程提交任务时用）
    void queueInLoop(Functor cb);  

    void wakeup();  // 通过eventfd唤醒阻塞的loop线程

    // EventLoop的方法 => Poller的方法（封装Poller的操作）
    void updateChannel(Channel *channel);  // 更新Channel的事件
    void removeChannel(Channel *channel);  // 移除Channel（
    bool hasChannel(Channel *channel);     // 检查Channel是否在Poller中

    // 判断EventLoop对象是否在自己的线程里
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); } // threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id

private:
    

}