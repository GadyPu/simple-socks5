#pragma once

#include<vector>
#include<string>

// 网络库底层的缓冲区类型定义
// prependable = readIndex
// readable = writeIndex - readIndex
// writable = size() - writeIndex
/*   +-------------+------------------+------------------+
*   | prependable |     readable     |     writable     |
*   +-------------+------------------+------------------+
*             read_index       writable_index          size
*/
const size_t kCheapPrepend =8;
const size_t kInitialSize =1024;
class  IoBuffer {

public:

    IoBuffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + initialSize)
        ,readerIndex_(kCheapPrepend)
        ,writerIndex_(kCheapPrepend)
    {}
    ~IoBuffer() {
        //buffer_.clear();
        /*
        首先用std::vector<int>()生成一个空的列表，其容量为0；
        然后用它和buf进行交换（swap方法）；
        因为空列表的内存占用是0，buf和它交换后就会被强制释放。
        之后再查看buf.capacity()，就会看到它也变成了0。
        */
        std::vector<char>().swap(buffer_);
    }
    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {
        return readerIndex_;
    }

    const char* peek() const {
        if (!readableBytes()) {
            return NULL;
        }
        return begin() + readerIndex_;
    }
    // 把onmessage函数上报的buffer数据,转成string类型的数据返回
    std::string retriveAllString() {
        return retriveAsString(readableBytes());    // 
    }

    // 读取指定长度数据到并返回string
    std::string retriveAsString(size_t len) {
        // fix bug 只读取可读取数据大小
        if (len > readableBytes()) {
            len = readableBytes();
        }
        std::string result(peek(), len);    // 将缓冲区中的可读数据取出成string
        retrive(len);       // 读出数据后,对已读buffer复位
        return result;
    }

    // buffer.size() - writeIndex_可写的buffer长度      len需要写的数据长度
    void ensureWritableBytes(size_t len) {
        if(writableBytes() < len) {
            makeSpace(len);     // 扩容函数
        }
    }

    // 把[data, data + len]内存上的数据添加到writable缓冲区当中
    void append(const char *data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    void seekReadIndex(size_t bytes) {
        // bytes 指外部使用peek()读取了bytes个数
        /*
        if ((readerIndex_ + bytes) < writerIndex_) {
            readerIndex_ += bytes;
        }
        */
        retrive(bytes);
    }

    void show_print() const {
        for (int i = 0; i < readableBytes(); i++) {
            printf("%d", *(peek() + i));
        }
        printf("\n");
    }

    char* beginWrite() {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const {
        return begin() + writerIndex_;
    }

private:
    // 返回缓冲区中可读数据的起始地址

    // onmessage  string<-buffer
    // 读取数据
    void retrive(size_t len) {
        if(len < readableBytes()) {
            readerIndex_ += len;     // 只读取了部分数据
        } else {
            retriveAll();       // 
        }
    }

    void retriveAll() {
        // 数据读取完毕重置位置指针
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    char* begin() {
        // it.operator*()取出迭代器的首元素
        return &*buffer_.begin();   
    }

    const char* begin() const {
        // it.operator*()取出迭代器的首元素
        return &*buffer_.begin();   
    }

    void makeSpace(size_t len) {
        // 当只读取部分数据时,readIndex_会向后移动大于 kCheapPrepend ,因此 prependableBytes() 读出的长度可能大于kCheapPrepend
        if(writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        } else {
            // move readable data to the front readerable buffer, to make behind space size for the len
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                        begin() + writerIndex_,
                        begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};