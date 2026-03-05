#pragma once
/**
 * noncopyable被继承后 派生类对象可正常构造和析构 但派生类对象无法进行拷贝构造和赋值构造
 **/
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete; // 禁止拷贝构造
    noncopyable &operator=(const noncopyable &) = delete;    // 禁止拷贝赋值
protected:
    noncopyable() = default; 
    ~noncopyable() = default;
};
