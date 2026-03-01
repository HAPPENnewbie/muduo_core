#pragma once

#include <iostream>
#include <string>

/*
    一个简单的时间戳封装类，用于高精度时间记录（微秒级）
*/

class Timestamp
{
public:
    // 默认构造，初始化为无效/零值时间戳
    Timestamp();
    // 显式构造：从已知的微秒值创建，禁止隐式类型转换防止误用
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    // 静态工厂方法：获取当前系统时间的 Timestamp 对象
    static Timestamp now();
    // 格式化为人类可读的字符串（如 "2024-01-15 08:30:45.123456"）
    std::string toString() const;

private:
    // 核心数据：64位有符号整数存储微秒级 Unix 时间戳
    int64_t microSecondsSinceEpoch_;
};