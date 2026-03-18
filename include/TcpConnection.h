#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/
// 继承 std::enable_shared_from_this<TcpConnection>：允许在类内部获取自身的 shared_ptr（避免悬空指针，比如在回调中使用 shared_from_this()）。
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &nameArg,
                  int sockfd,  // TCP 连接的文件描述符（accept 得到的 connfd）
                  const InetAddress &localAddr,   // 服务器端的 IP + 端口
                  const InetAddress &peerAddr);    // 客户端的 IP + 端口
    ~TcpConnection(); 

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 发送文件（零拷贝，高效传输大文件）
    void sendFile(int fileDescriptor, off_t offset, size_t count); 
    
    // 关闭半连接,（主动断开写端,触发 TCP 四次挥手的第一步）
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb)
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb)
    { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE
    {
        kDisconnected, // 已经断开连接
        kConnecting,   // 正在连接
        kConnected,    // 已连接
        kDisconnecting // 正在断开连接
    };
    void setState(StateE state) { state_ = state; }

    // IO 事件处理函数（Channel 的回调目标）
    void handleRead(Timestamp receiveTime);   // 处理读事件（有数据可读）
    void handleWrite();                         // 处理写事件（缓冲区数据可写）
    void handleClose();                      // 处理关闭事件（对端断开）
    void handleError();                        // 处理错误事件（比如连接重置）

    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    EventLoop *loop_; // 这里是baseloop还是subloop由TcpServer中创建的线程数决定 若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;//连接是否在监听读事件

    // Socket Channel 这里和Acceptor类似    Acceptor 在 mainloop里面    TcpConnection 在 subloop 里面
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

     const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些回调TcpServer也有 用户通过写入TcpServer注册 TcpServer再将注册的回调传递给TcpConnection TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调
    CloseCallback closeCallback_; // 关闭连接的回调
    size_t highWaterMark_; // 高水位阈值

    // 数据缓冲区, 解决tcp粘包问题
    Buffer inputBuffer_;    // 接收数据的缓冲区
    Buffer outputBuffer_;   // 发送数据的缓冲区 用户send向outputBuffer_发
};
