#include <sys/epoll.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

// 必须加 Channel::，否则编译器会认为这是全局变量，而非Channel类的成员
const int Channel::kNoneEvent = 0; //空事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; //读事件
const int Channel::kWriteEvent = EPOLLOUT; //写事件  

// EventLoop包含Channel列表和Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)   // 该Channel所属的事件循环
    , fd_(fd)
    , events_(0)    // 该fd"感兴趣"的事件（需要监听的事件）
    , revents_(0)   // Poller检测到的该fd"实际发生"的事件
    , index_(-1)   // 标记fd在Poller中的状态（如-1表示未注册，1表示已注册等，由Poller定义）
    , tied_(false)   // 是否绑定了TcpConnection的智能指针（解决生命周期问题）
{
}

Channel::~Channel()
{
    // 析构函数为空：Channel不管理fd的关闭，fd的生命周期由TcpConnection等上层对象管理
}


/**
 *Channel 是 TcpConnection 的成员变量，但 Channel 的回调函数（如 readCallback_）会访问 TcpConnection 的成员。如果 TcpConnection 已经销毁，而 Channel 还在处理事件，就会导致野指针访问。
 **/
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;   // 弱引用指向TcpConnection对象
    tied_ = true;  // // 标记已绑定
}


/**
 * 当改变channel所表示的fd的events事件后，update负责再poller里面更改fd相应的事件epoll_ctl
 **/
void Channel::update()
{
    loop_->updateChannel(this);
}

/**
 *   在channel所属的EventLoop中把当前的channel删除掉
 **/
void Channel::remove()
{
    loop_->removeChannel(this);
}

/**
 *   fd检测到事件，channel触发之前注册好的回调
 **/
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        // 指向void的智能指针是通用指针，可以指向任意类型的对象
        std::shared_ptr<void> guard = tie_.lock();  // 如果tie_绑定的对象还活，将tie_转为强指针
        if (guard)   //  存活则接着处理
        {
            handleEventWithGuard(receiveTime);
        }
        // 如果提升失败了 就不做任何处理 说明Channel的TcpConnection对象已经不存在了
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}


/**
 *   ，channel触发之前注册好的回调的具体实现
 **/
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    // 打印日志，记录当前触发的事件类型
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    // 关闭,EPOLLHUP说明fd被挂起（调用 close() 或 shutdown(SHUT_WR)使得fd本身被关闭），但没有EPOLLIN读事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) 
    {
        if (closeCallback_)  //  如果回调不为空则执行回调
        {
            closeCallback_();
        }
    }
    // 错误
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    // 读  EPOLLIN：普通读事件     EPOLLPRI：紧急读事件
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    // 写
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}