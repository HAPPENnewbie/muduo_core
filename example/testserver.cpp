 #include <string>

// #include "TcpServer.h"  // 封装的TCP服务器类
// #include "Logger.h"     // 日志工具类

class EchoServer 
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
    : server_(loop, addr, name),  
    loop_(loop)
    {
        // 注册回调函数
        server_.
    }

private:
    // 连接建立或断开的回调函数
    void onConnection() {

    }

    //  可读写事件回调
    void onMessage() {

    }

    TcpServer server_;
    EventLoop *loop_;

};



int main() {
    
}
