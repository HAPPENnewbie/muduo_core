#pragma once

#include <vector>   // 用于存储epoll_event事件列表
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

/**
 * epoll的使用:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);  // 构造：创建epoll实例（epoll_create），初始化epollfd_
    ~EPollPoller() override;   // 因为override会检查是否重写，所以这里加上override是为了强制检查是否写错了子类的析构函数

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;  // epoll_wait
    void updateChannel(Channel *channel) override;  // epoll_ctl (add, mod)
    void removeChannel(Channel *channel) override;  // epoll_ctl (add, del)

private:
    static const int kInitEventListSize = 16; // epoll事件列表的初始大小，即EventList的初始大小（避免频繁扩容

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道 其实就是调用epoll_ctl
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>; // C++中可以省略struct 直接写epoll_event即可

    int epollfd_;        // epoll实例的文件描述符（epoll_create返回值）
    EventList events_;   // 存储epoll_wait返回的「就绪事件」的容器
};