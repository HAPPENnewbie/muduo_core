#include <functional>
#include <string.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

// 辅助函数：检查EventLoop指针是否为空，为空则触发致命日志
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


// TcpServer构造函数：初始化服务器核心参数
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop))    // 初始化主事件循环（mainLoop）并做非空校验
    , ipPort_(listenAddr.toIpPort())      // 录服务器监听的IP和端口
    , name_(nameArg)     // 记录服务器名称
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)) //  创建Acceptor对象，负责监听客户端连接
    , threadPool_(new EventLoopThreadPool(loop, name_)) // 创建事件循环线程池，管理subLoop
    , connectionCallback_()  // 初始化连接回调（用户自定义）
    , messageCallback_()    // 初始化消息回调（用户自定义）
    , nextConnId_(1)   // 初始化连接ID生成器，用于唯一标识每个连接
    , started_(0)   // 初始化服务器启动标识，防止重复启动
{
    // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

// 释放所有资源，优雅关闭所有连接
TcpServer::~TcpServer()
{
    // 1.遍历所有已建立的连接
    for(auto &item : connections_)
    {
        // 2.将原始智能指针赋值给栈上的conn，延长对象生命周期
        TcpConnectionPtr conn(item.second);
        // 3. 复位原始智能指针，避免野指针
        item.second.reset();    
        // 4. 在连接所属的subLoop中执行连接销毁逻辑（保证线程安全）
        //    connectDestroyed会清理Channel、关闭socket、触发断开回调
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    int numThreads_=numThreads;
    threadPool_->setThreadNum(numThreads_);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_.fetch_add(1) == 0)    // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_);    // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新用户连接，acceptor会执行这个回调操作，负责将mainLoop接收到的请求连接(acceptChannel_会有读事件发生)通过回调轮询分发给subLoop去处理
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
   // 轮询算法 选择一个subLoop 来管理connfd对应的channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // 这里没有设置为原子类是因为其只在mainloop中执行 不涉及线程安全问题
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);
    // 根据连接成功的sockfd, 创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop,  
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer => TcpConnection的，至于Channel绑定的则是TcpConnection设置的四个，handleRead,handleWrite... 这下面的回调用于handlexxx函数中
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}