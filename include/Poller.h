#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller
{
public:
    using ChannelList = std::vector<Channel *>;  // 存储channel列表

    Poller(EventLoop *loop);
    virtual ~Poller() = default;    // 虚析构函数（基类必须），默认实现


    // 给所有IO复用保留统一的接口
    // 相当于启动epool_wait.等待事件就绪，timeoutMs是超时时间(毫秒)，activeChannels返回就绪的Channel
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0; 
    // 更新Channel的感兴趣事件（如新增/修改fd的监听事件）
    virtual void updateChannel(Channel *channel) = 0;
    // 移除Channel（停止监听fd的事件）
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前的Poller当中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现。注意这里为什么不让基类引用派生类（不规范），所以Poller.cpp里没有这个函数的实现。实现放到DefaultPoller.cpp里了
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // map的key:sockfd value:sockfd所属的channel通道类型. 通过这个 map 快速找到 fd 对应的 Channel，避免遍历
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};