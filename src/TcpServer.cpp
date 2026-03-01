#include <functional>
#include <string.h>

#include "TcpServer.h"
// #include "Logger.h"
// #include "TcpConnection.h"


// 校验传入的主线程 EventLoop（baseLoop）是否为空，为空则直接触发致命日志并退出；
static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

// 构造函数
TcpServer::TcpServer(EventLoop *loop,
            const InetAddress &listenAddr,
            const std::string &nameArg,
            Option option)
    : loop_(CheckLoopNotNull(loop))    // 校验并初始化baseLoop
    , ipPort_(listenAddr.toIpPort())   // 服务器监听的IP:Port（如127.0.0.1:8080）
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))  // 创建监听新连接的Acceptor
    , threadPool_(new EventLoopThreadPool(loop, name_)) // 创建事件循环线程池
    , connectionCallback_()    // 初始化连接回调（空函数）
    , messageCallback_()       // 初始化消息回调（空函数）
    , nextConnId_(1)             // 连接ID生成器，从1开始
    , started_(0)                 // 服务器启动标记（原子变量，初始0）
{
    // 给 Acceptor 设置 newConnectionCallback—— 当 Acceptor 检测到新连接（accept() 成功）时，会调用 TcpServer::newConnection 处理这个新连接；
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}


// 析构函数 ： 安全销毁所有活跃的 TCP 连接，避免资源泄漏；
TcpServer::~TcpServer()
{
    for(auto &item : connections_)  // 遍历保存活跃链接的哈希表
    {
        TcpConnectionPtr conn(item.second);  // 拷贝智能指针，增加引用计数
        item.second.reset();    // 把原始的智能指针复位 让栈空间的TcpConnectionPtr conn指向该对象 当conn出了其作用域 即可释放智能指针指向的对象
        // 销毁连接：必须在conn所属的subLoop线程中执行
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    numThreads_=numThreads;
    threadPool_->setThreadNum(numThreads_);
}


// 开启服务器监听
void TcpServer::start()
{
    if (started_.fetch_add(1) == 0)    // 原子操作，保证只启动一次。防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_);    // 启动线程池（创建subLoop线程）
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); // 启动监听
    }
}


// 处理新连接
// 有一个新用户连接，acceptor会执行这个回调操作，负责将mainLoop接收到的请求连接(acceptChannel_会有读事件发生)通过回调轮询分发给subLoop去处理
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
   // 轮询算法：选择一个subLoop管理新连接
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_); 
    ++nextConnId_;  // 连接ID自增，确保每个连接名字唯一
    std::string connName = name_ + buf; // 生成唯一的连接名（如MyServer-127.0.0.1:8080#1）

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 获取本机绑定的IP和端口（通过sockfd）
    sockaddr_in local;             // 定义 IPv4 地址结构体
    ::memset(&local, 0, sizeof(local));   // 清零，避免脏数据
    socklen_t addrlen = sizeof(local);    // 设置结构体长度（传入&传出参数）
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)  // 获取 sockfd 绑定的本地地址
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);   // 封装成自定义的 InetAddress 类

    
    // 创建TcpConnection对象，封装新连接
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn; // 保存到哈希表，管理连接生命周期

    // 把用户注册给TcpServer的回调，传递给TcpConnection
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置连接关闭的回调：当TcpConnection检测到连接断开时，调用TcpServer::removeConnection
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 在subLoop线程中初始化连接（触发连接建立回调）
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn));
}

// 移除连接,对外的封装接口，保证移除操作在 baseLoop 线程中执行（线程安全）；
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    // 把移除操作放到baseLoop线程中执行（线程安全）
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// 移除连接,真正执行移除逻辑；
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name()); // 从哈希表中删除连接
    EventLoop *ioLoop = conn->getLoop();
    // 在subLoop线程中销毁连接（触发connectDestroyed）
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}