#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Buffer.h"

/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读取数据的时候 却不知道tcp数据的最终大小
 *
 * @description: 从socket读到缓冲区的方法是使用readv先读至buffer_，
 * Buffer_空间如果不够会读入到栈上65536个字节大小的空间，然后以append的
 * 方式追加入buffer_。既考虑了避免系统调用带来开销，又不影响数据的接收。
 **/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // 栈上开辟64KB临时空间（65536字节），用于暂存Buffer装不下的数据
    char extrabuf[65536] = {0}; // 栈上内存空间 65536/1024 = 64KB

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    // 获取Buffer当前可写空间大小
    const size_t writable = writableBytes(); 

    // 第一块iovec缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块iovec缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 如果Buffer可写空间 < 64KB，就用2个缓冲区（Buffer+栈）；否则只用Buffer    
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    // readv：从fd读取数据，分散写入vec指向的多个缓冲区，返回实际读取的字节数
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)     // 读取失败：保存错误码（供上层处理）
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了n-writable长度的数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 对buffer_扩容 并将extrabuf存储的另一部分数据追加至buffer_
    }
    return n;
}

// inputBuffer_.readFd表示将对端数据读到inputBuffer_中，移动writerIndex_指针
// outputBuffer_.writeFd标示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    // 从Buffer的可读区域（peek()返回起始地址）读取数据，写入fd
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}