#include <time.h>

#include "Timestamp.h"

// 默认构造：初始化为 0
Timestamp::Timestamp() : microSecondsSinceEpoch_(0)
{
}

// 获取当前时间：调用 time() 获取秒级时间戳，转为微秒存入
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

// 获取当前时间：调用 time() 获取秒级时间戳，转为微秒存入
// 注意：time(NULL) 返回秒级，这里乘以 1000000 转为微秒，但精度实际只有秒级
Timestamp Timestamp::now()
{
    return Timestamp(time(NULL));
}

// 格式化为本地时间字符串
// 警告：localtime() 使用静态缓冲区，非线程安全
// 多线程环境下应改用 localtime_r()（POSIX）或 localtime_s()（Windows）
std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// #include <iostream>
// int main() {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }