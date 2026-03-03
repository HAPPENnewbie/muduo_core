#pragma once

#include <memory>
#include <functional>

class Buffer;    // 前置声明：缓冲区类（存储收发数据）
class TcpConnection;    // 前置声明：TCP连接类（封装连接的核心信息）
class Timestamp;    
 
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;  //高水位标记回调函数

using MessageCallback = std::function<void(const TcpConnectionPtr &,
                                           Buffer *,
                                           Timestamp)>;