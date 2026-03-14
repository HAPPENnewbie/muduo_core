#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>    // 信号量头文件，用于线程同步


// 初始化Thread类的静态原子变量，统计创建的线程总数
std::atomic_int Thread::numCreated_(0);


Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false) // 初始状态：未启动
    , joined_(false)  // 初始状态：未join

    , tid_(0)     // 初始tid：0（无效值）
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName(); // 初始化线程名称（用户未指定则生成默认名称）
}

Thread::~Thread()
{
    if (started_ && !joined_)   // 线程已启动且未被join，设置为分离线程
    {
        thread_->detach();                                                  // thread类提供了设置分离线程的方法 线程运行后自动销毁（非阻塞）
    }
}


// 一个Thread对象 记录的就是一个新线程的详细信息
void Thread::start()                                                       
{
    started_ = true;  // 1. 标记线程已启动，避免重复调用start()
    // 2. 初始化信号量：用于主线程和新线程的同步
    sem_t sem;
    sem_init(&sem, false, 0);                                            
    // 3. 创建C++11 std::thread对象，启动新线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid();                      // 3.1 新线程执行的第一步：获取并保存自己的tid
        sem_post(&sem);              // 3.2 发送信号量：通知主线程“tid已赋值完成”
        func_();                                        // 3.3 执行线程的核心逻辑（即之前绑定的threadFunc）
    }));

    // 这里必须等待获取上面新创建的线程的tid值, 避免 “主线程先执行、tid 还没赋值就读取” 的问题；
    sem_wait(&sem);
}

// C++ std::thread 中join()和detach()的区别：https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

// 设置默认线程名称
void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())  // 用户未指定名称时，生成默认名称
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}