#pragma once

// 标准库依赖：函数对象、字符串、智能指针、原子变量、哈希表
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// muduo 核心组件依赖
#include "EventLoop.h"          // 事件循环（Reactor 核心）
// #include "Acceptor.h"           // 监听套接字封装（处理新连接）
#include "InetAddress.h"        // IP+端口封装
#include "noncopyable.h"        // 不可拷贝基类（禁止对象拷贝）
// #include "EventLoopThreadPool.h"// 事件循环线程池（管理subloop）
#include "Callbacks.h"          // 回调函数类型定义
// #include "TcpConnection.h"      // TCP连接封装
// #include "Buffer.h"             // 数据缓冲区（解决粘包/拆包）

// 对外的服务器编程使用的类
class TcpServer
{
public:
    // 线程初始化回调类型：subloop线程启动时触发，用于初始化子线程的 EventLoop
    using TreadInitCallback = std::function<void(EventLoop *)>;

    // 枚举类型端口重用选项（SO_REUSEPORT）
    enum Option
    {
        kNoReusePort,  //不允许重用本地端口
        kReusePort,    //允许重用本地端口
    };

    // 构造函数：创建服务器对象
    // 参数：mainloop（主线程事件循环）、监听地址、服务器名称、端口重用选项
    TcpServer(EventLoop *loop,
            const InetAddress &listenAddr,
            const std::string &nameArg,
            Option option = kNoReusePort);
    ~TcpServer();

    // 注册各类回调函数
    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 设置subloop线程数（工作线程数）
    void setThreadNum(int numThreads);

    // 启动服务器（监听端口+初始化线程池）
    void start();

private:
    // 当 Acceptor 监听到新连接时的回调（由 Acceptor 调用）
    void newConnection(int sockfd, const InetAddress & peerAddr); 
    // 移除连接（对外接口，线程安全） 
    void removeConnection(const TcpConnectionPtr &conn);
    // 移除连接的核心逻辑（必须在 conn 所属的 EventLoop 线程中执行）
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    
    // 哈希表：保存所有已建立的 TCP 连接（key：连接名，value：TcpConnection 智能指针）
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseLoop（主线程的 EventLoop，用户传入）
    const std::string ipPort_; // 服务器监听的 IP:Port（如 127.0.0.1:8080）
    const std::string name_;   // 服务器名称
    std::unique_ptr<Acceptor> acceptor_; // 监听新连接的对象（运行在 baseLoop）
    std::shared_ptr<EventLoopThreadPool> threadPool_; // 事件循环线程池（管理 subloop）

    // 回调函数
    ConnectionCallback connectionCallback_;  //有新连接时的回调
    MessageCallback messageCallback_;            // 有读写事件发生时的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成后的回调
    ThreadInitCallback threadInitCallback_;   // loop线程初始化的回调

    int numThreads_; // 线程池的线程数量
    std::atomic_int started_; // 原子变量：标记服务器是否已启动（0：未启动，1：已启动）
    int nextConnId_; // 连接 ID 生成器（递增，用于给每个连接分配唯一名称）
    ConnectionMap connections_; // 保存所有活跃的 TCP 连接
}
