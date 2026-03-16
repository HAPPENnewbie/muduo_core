#pragma once

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

#include "noncopyable.h"

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;  //  线程入口函数，表示线程实际要做的任务。

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();   // 启动线程
    void join();     // 线程是否已被join

    bool started() { return started_; }     // 判断线程是否已启动
    pid_t tid() const { return tid_; }        // 获取线程ID
    const std::string &name() const { return name_; }         // 获取线程名称

    static int numCreated() { return numCreated_; }   // 获取已创建的线程总数（静态函数）返回全局的线程创建总数（静态变量 numCreated_），因为是静态函数，无需创建 Thread 对象即可调用。

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;       // 在线程创建时再绑定
    ThreadFunc func_; // 线程要执行的回调函数
    std::string name_; 
    static std::atomic_int numCreated_;  // std::atomic_int 替代普通 int，保证多线程创建 Thread 对象时，numCreated_ 的计数不会出错（无需加锁）
};