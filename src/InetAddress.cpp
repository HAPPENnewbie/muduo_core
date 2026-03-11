#include <strings.h>  // 提供 memset（内存置零）
#include <string.h>

#include "InetAddress.h"

// 根据你传入的「端口号」和「IP 字符串」，初始化底层的 sockaddr_in 结构体（addr_），让这个结构体符合 Linux 网络编程的规范，能直接被 bind/connect 等 socket 系统调用使用
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    ::memset(&addr_, 0, sizeof(addr_));   // 把 addr_ 结构体的所有字节置 0，防止内存中残留随机值导致错误
    addr_.sin_family = AF_INET;  // 固定设置为 AF_INET，表示这是 IPv4 地址
    addr_.sin_port = ::htons(port); // 本地字节序转为网络字节序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  // 点分十进制的 IP 字符串（如 "127.0.0.1"）转换成网络字节序的 32 位整数
}
 

// 把存储在 addr_ 中的「网络字节序二进制 IP 地址」（比如 0x7F000001）转换成人类可读的「点分十进制字符串」（比如 "127.0.0.1"），方便日志输出、调试或展示给用户。
std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

// 把存储在 addr_ 中的「IP 地址 + 端口号」组合成人类可读的 IP:端口 格式字符串（比如 "127.0.0.1:8080"）
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = ::strlen(buf);
    uint16_t port = ::ntohs(addr_.sin_port);
    sprintf(buf+end, ":%u", port);
    return buf;
}

// 把存储在 addr_ 中的「网络字节序端口号」转换成「主机字节序的 16 位整数」并返回
uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port);
}

// #if 0
// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
// }
// #endif