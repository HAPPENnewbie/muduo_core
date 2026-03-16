#pragma once

#include "noncopyable.h"

class InetAddress;  // 封装 IP 地址和端口的类

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }
    ~Socket();

    int fd() const { return sockfd_; }  
    // 绑定 socket 到指定的 IP 地址和端口（对应 bind() 系统调用）
    void bindAddress(const InetAddress &localaddr);
    
    // 监听 socket，使其变为被动套接字，等待客户端连接（对应 listen() 系统调用）
    void listen();

    // 接受客户端连接（对应 accept() 系统调用）
    // 参数 peeraddr 用于存储客户端的地址信息（IP+端口）
    // 返回值是新的 socket fd（用于和该客户端通信）
    int accept(InetAddress *peeraddr);

    // 关闭 socket 的写端（对应 shutdown() 系统调用，参数 SHUT_WR）
    // 常用于优雅关闭连接：告诉对方“我不再发数据了，但还能收”
    void shutdownWrite();

    // 更改tcp选项的一些函数
    void setTcpNoDelay(bool on);  // 直接发送，不进行tcp缓冲
    void setReuseAddr(bool on);   // 
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};
