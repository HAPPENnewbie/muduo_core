#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

// Channel的三种状态（通过index_标记，避免重复add/del）
const int kNew = -1;    // Channel未添加到Poller（初始状态）, channel的index_初始化也是-1，默认为未添加到Poller
const int kAdded = 1;   // Channel已添加到Poller
const int kDeleted = 2; // Channel已从Poller删除

// 构造函数：创建 epoll 实例
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)  // 调用基类Poller的构造函数，把基类的成员给初始化了
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC)) // epoll_create1 比传统的 epoll_create 更安全
    , events_(kInitEventListSize) // vector<epoll_event>(16)
{
    if (epollfd_ < 0)  // // 创建失败则直接终止程序（FATAL级别日志）
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}


// 析构函数：释放 epoll 资源
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);  // // 关闭epoll实例的fd，释放内核资源
}


// 监听事件，填充就绪事件
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 打印当前Poller管理的Channel数量（调试用）
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    // 调用epoll_wait等待事件：返回的是准备好的事件数量。内核会把就绪事件的详细内容传到events_容器里
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;   // 其他eventloop执行可能会修改errno的值，避免 errno 被后续操作覆盖，导致无法准确判断 epoll_wait 的失败原因
    Timestamp now(Timestamp::now());

    if (numEvents > 0)  // 有已经发生的事件
    {
        LOG_INFO("%d events happend\n", numEvents); // LOG_DEBUG最合理
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) // 扩容操作
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)  //  这一轮监听没有发现事件，知识timeout超时返回了
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)   // 不是外部中断，有其他错误引起
        {
            errno = saveErrno;   //  又把saveErrno 赋值给errno, 目的是为了适配日志输出errno
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;   // 返回时间发生的事件点
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
// 更新 Channel 的事件，epoll_ctl
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); // // 获取Channel当前状态（kNew/kAdded/kDeleted)
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) // // 未添加 或 已删除（
    {
        if (index == kNew)  // 全新的Channel，先加入Poller的管理表
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        else // index == kDeleted
        {
        }
        // 无论kNew还是kDeleted，都标记为已添加，并调用ADD
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在Poller中注册过了
    {
        int fd = channel->fd();     
        if (channel->isNoneEvent()) // Channel不再关注任何事件（events()=0）
        {
            // 移除该fd的监听，标记为已删除
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else  // Channel修改了关注的事件（如从读事件改为写事件）
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除channel,epoll_ctl
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接，其实就是poll函数后返回了就绪事件，然后根据这些就绪事件将对应的channel放到就绪channel的集合中
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);  // 把发生的事件填到channel
        activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给它返回的所有发生事件的channel列表了
    }
}


// 更新channel通道 其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    ::memset(&event, 0, sizeof(event));  // // 初始化epoll_event（置零避免脏数据）

    int fd = channel->fd();

    event.events = channel->events();  // 设置要监听的事件（如EPOLLIN/EPOLLOUT
    event.data.fd = fd;
    event.data.ptr = channel;          // 关键：存入Channel指针，方便后续回调

    // 调用epoll_ctl：操作类型（ADD/MOD/DEL）、fd、事件结构体
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}