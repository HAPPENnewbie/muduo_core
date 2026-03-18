#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;//初始预留的prependabel空间大小
    static const size_t kInitialSize = 1024;  // 初始可写空间大小

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }
    // 可读字节数 = 写指针 - 读指针（已写入未读取的数据长度）
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    // 可写字节数 = 缓冲区总大小 - 写指针（剩余可写入的空间）
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    // 可预留字节数 = 读指针（Prependable区域的总大小，含已用/未用）
    size_t prependableBytes() const { return readerIndex_; }
    // 返回缓冲区中可读数据的起始地址
    const char *peek() const { return begin() + readerIndex_; }

    // 读取len字节后，移动读指针
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 说明应用只读取了可读缓冲区数据的一部分，就是len长度 还剩下readerIndex+=len到writerIndex_的数据未读
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }

    // 复位读写指针
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 读取所有可读数据并转为string，同时清空缓冲区
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    // 读取len字节并转为string，同时移动读指针
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    // 确保可写空间至少有len字节，不足则扩容
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
    }

    // 写入数据到可写区域
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }

    // 返回可写区域的起始地址（供用户写入数据）
    char *beginWrite() { return begin() + writerIndex_; } // 非const版本：允许修改缓冲区（写入数据）
    const char *beginWrite() const { return begin() + writerIndex_; } // const版本：只读访问，不修改缓冲区

    // 从fd（如socket）读取数据到缓冲区
    ssize_t readFd(int fd, int *saveErrno);
    // 将缓冲区数据写入fd（如socket）
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // vector底层数组首元素的地址 也就是数组的起始地址
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        // 情况1：可写空间 + 未使用的Prependable空间 < 需要的空间 + 预留空间
        // （剩余总空闲空间不足，直接扩容）
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)  // 扩容到「当前写位置 + 需要的空间」
        {
            buffer_.resize(writerIndex_ + len);
        }
        else    // 情况2：剩余空闲空间足够，整理内存（将可读数据移到Prependable区域后，腾出连续可写空间）
        {
            size_t readable = readableBytes(); // readable = reader的长度
            // 将当前缓冲区中从readerIndex_到writerIndex_的数据
            // 拷贝到缓冲区起始位置kCheapPrepend处，以便腾出更多的可写空间
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;            // 数据可读的下标
    size_t writerIndex_;            //  数据可写的下标
};
