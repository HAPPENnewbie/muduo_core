#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop; // 因为 Channel 只用到 EventLoop* 指针，无需包含完整定义，减少编译依赖。

/**
 * 理清楚 EventLoop、Channel、Poller之间的关系  Reactor模型上对应多路事件分发器
 * Channel理解为通道 封装了sockfd和其感兴趣的event 如EPOLLIN、EPOLLOUT事件 还绑定了poller返回的具体事件。是 Reactor 模型中事件分发的核心载体
 **/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>; //  通用事件回调（无参数）
    using ReadEventCallback = std::function<void(Timestamp)>; // 只读事件回调（带时间戳）

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 当 Poller 检测到 fd 有事件发生时，EventLoop 会调用该函数；随后会根据 revents_触发对应的回调（读 / 写 / 关闭 / 错误）
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当channel被手动remove掉 channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);
 
    int fd() const { return fd_; }  // 一个 Channel 绑定一个 fd
    int events() const { return events_; }  // 返回感兴趣的事件
    void set_revents(int revt) { revents_ = revt; }

    // 事件状态控制:设置fd相应的事件状态，这里是通过位运算控制点 相当于epoll_ctl add delete。修改完之后还只是在channel里面修改，poller还不知道，所以需要update同步到poller
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 事件状态查询:返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }   // true, 当前fd无任何感兴趣事件
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // Poller 索引管理
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //  EventLoop 关联与 Channel 移除
    EventLoop *ownerLoop() { return loop_; }  // 返回所属的 EventLoop，确保 “one loop per thread”
    void remove();   // 从 EventLoop/Poller 中移除当前 Channel,会调用 Poller 的删除接口，取消对该 fd 的监听。

private:
    void update(); // 封装对 Poller 的更新操作
    void handleEventWithGuard(Timestamp receiveTime);

    // 当前fd的状态
    static const int kNoneEvent; // 无事件
    static const int kReadEvent;  // 读事件（如 EPOLLIN）
    static const int kWriteEvent; // 写事件（如 EPOLLOUT）
 
    EventLoop *loop_; // 事件循环
    const int fd_;    // fd，Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // Poller返回的具体发生的事件
    int index_;       // Poller 内部索引

    std::weak_ptr<void> tie_;     // 绑定上层对象的弱指针（防止回调悬空）
    bool tied_;                   // 是否已调用 tie() 绑定对象

    // 因为channel通道里可获知fd最终发生的具体的事件events，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};