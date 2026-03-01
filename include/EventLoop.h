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

}