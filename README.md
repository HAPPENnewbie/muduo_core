# muduo-core

## 项目介绍
本项目是参考 muduo 实现的基于 多Reactor 模型的多线程网络库。使用 C++ 11 编写去除 muduo 对 boost 的依赖。
项目已经实现了 Channel 模块、Poller 模块、事件循环模块、日志模块、线程池模块等功能。

## 开发环境
* linux kernel version5.15.0-113-generic (ubuntu 22.04.6)
* gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
* cmake version 3.22


## 功能介绍
- **事件轮询与分发模块**：`EventLoop.*`、`Channel.*`、`Poller.*`、`EPollPoller.*`负责事件轮询检测，并实现事件分发处理。`EventLoop`对`Poller`进行轮询，`Poller`底层由`EPollPoller`实现。
- **线程与事件绑定模块**：`Thread.*`、`EventLoopThread.*`、`EventLoopThreadPool.*`绑定线程与事件循环，完成`one loop per thread`模型。
- **网络连接模块**：`TcpServer.*`、`TcpConnection.*`、`Acceptor.*`、`Socket.*`实现`mainloop`对网络连接的响应，并分发到各`subloop`。
- **缓冲区模块**：`Buffer.*`提供自动扩容缓冲区，保证数据有序到达。

## 技术亮点
1. **高并发非阻塞网络库**  
   `muduo`采用`Reactor`多模型多线程的结合，实现了高并发非阻塞的网络库。
2. **智能指针防止悬空指针**  
   `TcpConnection`继承自`enable_shared_from_this`，其目的是防止在不该被释放对象的地方释放对象，导致悬空指针的产生。  
   这样可以避免用户可能在处理`OnMessage`事件时删除对象，确保`TcpConnection`以正确方式释放。
3. **唤醒机制**  
   `EventLoop`中使用了`eventfd`来调用`wakeup()`，让`mainloop`唤醒`subloop`的`epoll_wait`阻塞。
4. **线程创建有序性**  
   在`Thread`中通过`C++ lambda`表达式以及信号量机制，保证线程创建的有序性，确保线程正常创建后再执行线程函数。
5. **非阻塞核心缓冲区**  
   `Buffer.*`是`muduo`网络库非阻塞的核心模块。当触发相应的读写事件时，内核缓冲区可能没有足够空间一次性发送数据，此时有两种选择：  
   - 第一种是将其设置为非阻塞，但可能造成 CPU 忙等待；  
   - 第二种是阻塞等待内核缓冲区有空间再发送，但效率低下。  
   为了解决这些问题，`Buffer`模块将多余数据存储在用户缓冲区，并注册相应的读写事件监听，待事件再次触发时统一发送。



## 参考资料

- [作者-Shangyizhou]https://github.com/Shangyizhou/A-Tiny-Network-Library/tree/main
- [作者-S1mpleBug]https://github.com/S1mpleBug/muduo_cpp11?tab=readme-ov-file
- [作者-chenshuo]https://github.com/chenshuo/muduo
- 《Linux高性能服务器编程》
- 《Linux多线程服务端编程：使用muduo C++网络库》
