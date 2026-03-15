#include <memory>

#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "Logger.h"


// EventLoopThreadPool构造函数：初始化线程池核心成员变量
// baseLoop：主线程（主Reactor）对应的EventLoop，numThreads_默认初始化为0，next_用于轮询分配的下标
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0)
{
}

// EventLoopThreadPool析构函数：空实现
// 注：不删除EventLoop对象，因为EventLoop是栈上变量（由对应的线程管理生命周期），避免野指针和重复释放
EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
}


// 启动线程池：创建指定数量的EventLoopThread（子线程），并初始化每个子线程的EventLoop
// cb：线程初始化回调函数，用于子线程EventLoop创建完成后执行自定义初始化逻辑
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    // 1. 标记线程池已启动
    started_ = true;
    // 2. 循环创建numThreads_个子线程（每个线程对应一个subReactor/EventLoop）
    for (int i = 0; i < numThreads_; ++i)
    {
        // 2.1 构造子线程名称（线程池名称+序号）
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        // 2.2 创建EventLoopThread对象：封装了线程和对应的EventLoop
        EventLoopThread *t = new EventLoopThread(cb, buf);
        // 2.3 将EventLoopThread托管给unique_ptr，避免内存泄漏
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // 2.4 启动子线程并获取其EventLoop指针：
        //    - 底层创建操作系统线程，绑定新的EventLoop
        //    - 子线程启动后会阻塞等待，直到EventLoop初始化完成并返回指针
        //    - 将子线程的EventLoop指针存入loops_容器，供后续分配使用
        loops_.push_back(t->startLoop());
    }
    // 3. 特殊情况：如果线程池数量为0（单线程模式），直接在主线程的EventLoop执行初始化回调
    if (numThreads_ == 0 && cb) // 整个服务端只有一个线程运行baseLoop
    {
        cb(baseLoop_);
    }
}


// 获取下一个可用的EventLoop（核心分配逻辑）
// 核心逻辑：baseLoop（mainReactor）以轮询方式将Channel分配给subLoop（subReactor）
// 适用场景：多线程Reactor模型中，mainReactor负责接收连接，subReactor负责处理连接的读写事件
EventLoop *EventLoopThreadPool::getNextLoop()
{
    // 1. 默认返回主线程的baseLoop（单线程模式下始终返回这个）
    //    单线程模式：只有mainReactor，无subReactor，所有事件都由baseLoop处理
    EventLoop *loop = baseLoop_;    

    // 2. 多线程模式：轮询选择下一个subLoop
    //    如果loops_不为空（说明创建了子线程/子Reactor），则进入轮询逻辑
    if(!loops_.empty())             
    {
        // 2.1 获取当前轮询到的subLoop
        loop = loops_[next_];
        // 2.2 下标自增，准备下一次分配
        ++next_;
        // 2.3 下标越界时重置为0，实现循环轮询（Round-Robin）
        if(next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    // 3. 返回选中的EventLoop（单线程返回baseLoop，多线程返回轮询的subLoop）
    return loop;
}


// 获取所有可用的EventLoop
// 作用：提供给外部遍历所有EventLoop（比如批量操作、监控等场景）
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    // 1. 单线程模式：返回只包含baseLoop的vector
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    // 2. 多线程模式：直接返回所有subLoop的vector
    else
    {
        return loops_;
    }
}