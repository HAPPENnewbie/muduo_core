#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装socket地址类型,提供了一系列便捷的成员函数来获取 IP 地址、端口号，以及格式化输出 IP + 端口
class InetAddress
{
public:
    // 构造函数1：指定端口和IP（默认端口0，默认IP 127.0.0.1）
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    // 构造函数2：直接从已有的 sockaddr_in 结构体初始化
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }
    
    std::string toIp() const;  
    std::string toIpPort() const;
    uint16_t toPort() const;

    // 返回 addr_ 的常量指针，用于给 socket 系统调用
    const sockaddr_in *getSockAddr() const { return &addr_; }
    // 手动修改 addr_ 的值，用于动态更新地址信息
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};