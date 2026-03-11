#include <stdlib.h> // 提供 getenv() 函数，用于读取环境变量

#include "Poller.h"
#include "EPollPoller.h"  // EPollPoller 子类的头文件，epoll 实现的 Poller

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))  // 占位，实际应返回 其他多路复用，比如new PollPoller(loop)
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        // 默认创建 EPollPoller 实例，传入所属的 EventLoop
        return new EPollPoller(loop); // 生成epoll的实例
    }
}