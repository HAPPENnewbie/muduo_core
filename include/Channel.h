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

    // fd得到Poller通知以后 处理事件 handleEvent在EventLoop::loop()中调用
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当channel被手动remove掉 channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态 相当于epoll_ctl add delete
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();
private:

    void update();
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