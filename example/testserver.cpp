 #include <string>

#include "TcpServer.h"  // 封装的TCP服务器类
#include "Logger.h"     // 日志工具类

class EchoServer 
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
    : server_(loop, addr, name),  
    loop_(loop)
    {
        // 1. 注册连接回调函数：当客户端连接建立/断开时，调用onConnection
        server_.setConnetionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        // 2. 注册消息回调函数：当收到客户端数据时，调用onMessage
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
        // 3. 设置服务器的工作线程数（1个线程监听链接，其余的为工作线程）
        server_.setThreadNum(3);
    }

    void start() {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())   // 判断连接是否建立成功
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    //  接收客户端数据的回调函数
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        //  1. 从缓冲区读取所有数据并转为字符串
        std::string msg = buf->retrieveAllAsString();
        // 2. 将数据原样回传给客户端
        conn->send(msg);
        // conn->shutdown();   // 关闭写端 底层响应EPOLLHUP => 执行closeCallback_
    }

    TcpServer server_;  // 核心的TCP服务器对象
    EventLoop *loop_;    // 主事件循环（main loop）指针

};


int main() {
    EventLoop loop;  // 创建主事件循环（Reactor）
    InetAddress addr(8080); // 监听8080端口
    EchoServer server(&loop, addr, "EchoServer"); // 创建回声服务器
    server.start(); // 启动服务器
    loop.loop();   // epoll_wait 以阻塞方式等待新用户链接
    return 0;
}
