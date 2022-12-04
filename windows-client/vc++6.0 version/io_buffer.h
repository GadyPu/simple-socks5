#pragma once

#include<vector>
#include<string>

// �����ײ�Ļ��������Ͷ���
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
        ������std::vector<int>()����һ���յ��б�������Ϊ0��
        Ȼ��������buf���н�����swap��������
        ��Ϊ���б���ڴ�ռ����0��buf����������ͻᱻǿ���ͷš�
        ֮���ٲ鿴buf.capacity()���ͻῴ����Ҳ�����0��
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
    // ��onmessage�����ϱ���buffer����,ת��string���͵����ݷ���
    std::string retriveAllString() {
        return retriveAsString(readableBytes());    // 
    }

    // ��ȡָ���������ݵ�������string
    std::string retriveAsString(size_t len) {
        // fix bug ֻ��ȡ�ɶ�ȡ���ݴ�С
        if (len > readableBytes()) {
            len = readableBytes();
        }
        std::string result(peek(), len);    // ���������еĿɶ�����ȡ����string
        retrive(len);       // �������ݺ�,���Ѷ�buffer��λ
        return result;
    }

    // buffer.size() - writeIndex_��д��buffer����      len��Ҫд�����ݳ���
    void ensureWritableBytes(size_t len) {
        if(writableBytes() < len) {
            makeSpace(len);     // ���ݺ���
        }
    }

    // ��[data, data + len]�ڴ��ϵ�������ӵ�writable����������
    void append(const char *data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    void seekReadIndex(size_t bytes) {
        // bytes ָ�ⲿʹ��peek()��ȡ��bytes����
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
    // ���ػ������пɶ����ݵ���ʼ��ַ

    // onmessage  string<-buffer
    // ��ȡ����
    void retrive(size_t len) {
        if(len < readableBytes()) {
            readerIndex_ += len;     // ֻ��ȡ�˲�������
        } else {
            retriveAll();       // 
        }
    }

    void retriveAll() {
        // ���ݶ�ȡ�������λ��ָ��
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    char* begin() {
        // it.operator*()ȡ������������Ԫ��
        return &*buffer_.begin();   
    }

    const char* begin() const {
        // it.operator*()ȡ������������Ԫ��
        return &*buffer_.begin();   
    }

    void makeSpace(size_t len) {
        // ��ֻ��ȡ��������ʱ,readIndex_������ƶ����� kCheapPrepend ,��� prependableBytes() �����ĳ��ȿ��ܴ���kCheapPrepend
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