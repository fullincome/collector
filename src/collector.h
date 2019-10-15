#ifndef COLLECTOR_HPP
#define COLLECTOR_HPP

#include <cmath>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>


#define BYTES_SIZE 4
#define SIZE_COLLECTOR_MEMORY 500
#define MbToB(x) (x) * 1024 * 1024
#define collector_callback std::function<void(uint8_t *buf, size_t size)>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Ring buffer, not thread safe.
// Write pointer not reach read pointer.
// If read pointer reach write pointer - buffer is empty.
class RingBuffer
{
private:
    size_t read_pos_;
    size_t write_pos_;
    std::vector<char> buffer_;

public:
    RingBuffer();
    RingBuffer(size_t size);

    void resize(size_t size);
    size_t size();
    bool is_empty();
    bool push_back(std::string &str);
    bool push_back(uint8_t *s, size_t size);
    size_t pop_front(std::string &dest);
    size_t pop_front(uint8_t *dest, size_t &dest_size);
};

inline RingBuffer::RingBuffer() :
    read_pos_(0),
    write_pos_(0),
    buffer_(0)
{};

inline RingBuffer::RingBuffer(size_t size) :
    read_pos_(0),
    write_pos_(0),
    buffer_(size, 0)
{};

inline void RingBuffer::resize(size_t size)
{
    read_pos_ = 0;
    write_pos_ = 0;
    buffer_.resize(size);
}

inline size_t RingBuffer::size()
{
    return buffer_.size();
}

inline bool RingBuffer::is_empty()
{
    return read_pos_ == write_pos_;
}

// Write pointer cannot be closer than shown below:
//
// ta2 ] [x_data 3] _ [ xxxx_data 1 ] [ xx_da
//                  ^ ^
//                 wp rp
//
// At maximum capacity, 1 byte will be empty.
inline bool RingBuffer::push_back(std::string &str)
{
    if (str.size() >= size())
        return 0;
        
    size_t size_block = str.size();
    size_t size_block_with_len = size_block + BYTES_SIZE;
    size_t size_to_buffer_end = -1;
    size_t cur_pos = write_pos_;

    if (cur_pos < read_pos_)
    {
        if (cur_pos + size_block_with_len >= read_pos_)
            return 0;
    }
    else
    {
        if (cur_pos + size_block_with_len >= size())
        {
            if ((cur_pos + size_block_with_len) % size() >= read_pos_)
                return 0;

            // Set size_to_buffer_end only if BYTES_SIZE bytes
            // fit at end of a buffer. Otherwise, the pointer will
            // return to the begining when writing the size.
            if (cur_pos + BYTES_SIZE < size())
                size_to_buffer_end = size() - cur_pos - BYTES_SIZE;
        }
    }

    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        buffer_[cur_pos] = ((size_block >> (i * 8)) & 0XFF);
        cur_pos++;

        if (cur_pos == size())
            cur_pos = 0;
    }

    if (size_to_buffer_end == -1)
    {
        memcpy(buffer_.data() + cur_pos, str.c_str(), size_block);
        write_pos_ = cur_pos + size_block;
    }
    else
    {
        memcpy(buffer_.data() + cur_pos, str.c_str(), size_to_buffer_end);
        memcpy(buffer_.data(), str.c_str() + size_to_buffer_end, size_block - size_to_buffer_end);
        write_pos_ = size_block - size_to_buffer_end;
    }

    return true;
}

inline bool RingBuffer::push_back(uint8_t *s, size_t size_block)
{
    size_t size_block_with_len = size_block + BYTES_SIZE;
    size_t size_to_buffer_end = -1;
    size_t cur_pos = write_pos_;

    if (size_block >= size())
        return 0;

    if (cur_pos < read_pos_)
    {
        if (cur_pos + size_block_with_len >= read_pos_)
            return 0;
    }
    else
    {
        if (cur_pos + size_block_with_len >= size())
        {
            if ((cur_pos + size_block_with_len) % size() >= read_pos_)
                return 0;

            // Set size_to_buffer_end only if BYTES_SIZE bytes
            // fit at end of a buffer. Otherwise, the pointer will
            // return to the begining when writing the size.
            if (cur_pos + BYTES_SIZE < size())
                size_to_buffer_end = size() - cur_pos - BYTES_SIZE;
        }
    }

    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        buffer_[cur_pos] = ((size_block >> (i * 8)) & 0XFF);
        cur_pos++;

        if (cur_pos == size())
            cur_pos = 0;
    }

    if (size_to_buffer_end == -1)
    {
        memcpy(buffer_.data() + cur_pos, s, size_block);
        write_pos_ = cur_pos + size_block;
    }
    else
    {
        memcpy(buffer_.data() + cur_pos, s, size_to_buffer_end);
        memcpy(buffer_.data(), s + size_to_buffer_end, size_block - size_to_buffer_end);
        write_pos_ = size_block - size_to_buffer_end;
    }

    return true;
}

// Return read block size.
// Return block size in dest_size if dest_size too small.
inline size_t RingBuffer::pop_front(uint8_t *dest, size_t &dest_size)
{
    if (is_empty())
        return 0;

    // Read size of block data
    size_t size_block = 0;
    size_t size_to_buffer_end = -1;
    size_t cur_pos = read_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        size_block += (buffer_[cur_pos] << (8 * i));
        ++cur_pos;

        if (cur_pos == buffer_.size())
            cur_pos = 0;
    }

    if (dest_size < size_block)
    {
        dest_size = size_block;
        return 0;
    }

    if (cur_pos + size_block >= size())
        size_to_buffer_end = size() - cur_pos;

    if (size_to_buffer_end == -1)
    {
        // cur pos -> end
        memcpy(dest, buffer_.data() + cur_pos, size_block);
        read_pos_ = cur_pos + size_block;
    }
    else
    {
        // cur_pos -> end buffer; zero pos -> end;
        memcpy(dest, buffer_.data() + cur_pos, size_to_buffer_end);
        memcpy(dest, buffer_.data(), size_block - size_to_buffer_end);
        read_pos_ = size_block - size_to_buffer_end;
    }

    return size_block;
}

// Return read block size.
inline size_t RingBuffer::pop_front(std::string &dest)
{
    if (is_empty())
        return 0;

    size_t size_block = 0;
    size_t size_to_buffer_end = -1;
    size_t cur_pos = read_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        size_block += (buffer_[cur_pos] << (8 * i));
        ++cur_pos;

        if (cur_pos == buffer_.size())
            cur_pos = 0;
    }

    if (cur_pos + size_block >= size())
        size_to_buffer_end = size() - cur_pos;

    dest.reserve(size_block);
    if (size_to_buffer_end == -1)
    {
        // cur pos -> end
        dest.append(buffer_.data() + cur_pos, size_block);
        read_pos_ = cur_pos + size_block;
    }
    else
    {
        // cur_pos -> end buffer; zero pos -> end;
        dest.append(buffer_.data() + cur_pos, size_to_buffer_end);
        dest.append(buffer_.data(), size_block - size_to_buffer_end);
        read_pos_ = size_block - size_to_buffer_end;
    }

    return size_block;
}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
* Collector - RingBuffer wrapper.
* Thread safe, except set_size() and set_callback(),
* a single call both function is expected at the initialization stage.
*/

/*
* For more custom handler() use inheritance and override that.
*/
#define READ_TIMEOUT_MS 3000
class Collector
{
private:
    std::mutex _mutex;
    std::condition_variable _cv;
    RingBuffer buffer_;
    collector_callback _call_back;

protected:
    size_t get_size();
    virtual void handler();

public:
    Collector();
    Collector(size_t size);
    Collector(size_t size, collector_callback call_back);
    Collector(const Collector &collector);
    Collector &operator=(Collector &collector);

    void set_size(size_t size);
    void set_callback(collector_callback call_back);
    bool push(std::string &str);
    bool push(uint8_t *src, size_t size);
    // waitable_ms: < 0 - не ждем
    //              = 0 - ждем пока не будет данных
    //              > 0 - ждем пока не будет данных, или не истечет время waitable_ms
    size_t read(std::string &dest, int waitable_ms = READ_TIMEOUT_MS);
    size_t read(uint8_t *dest, size_t dest_size, int waitable_ms = READ_TIMEOUT_MS);
    bool start();
};

inline Collector::Collector() :
    buffer_(MbToB(SIZE_COLLECTOR_MEMORY))
{
    _call_back = NULL;
}

inline Collector::Collector(size_t size) :
    buffer_(size)
{
    _call_back = NULL;
}

inline Collector::Collector(size_t size, collector_callback call_back) :
    buffer_(size),
    _call_back(call_back)
{}

inline Collector::Collector(const Collector &collector) :
    buffer_(collector.buffer_),
    _call_back(collector._call_back)
{}

inline Collector &Collector::operator=(Collector &collector)
{
    buffer_ = collector.buffer_;
    _call_back = collector._call_back;
    return *this;
};

inline size_t Collector::get_size()
{
    return buffer_.size();
}

inline void Collector::set_size(size_t size)
{
    buffer_.resize(size);
}

inline void Collector::set_callback(collector_callback call_back)
{
    _call_back = call_back;
}

inline bool Collector::push(std::string &str)
{
    if (get_size() - BYTES_SIZE - 1 <= str.size())
    {
        return 0;
    }

    std::unique_lock<std::mutex> lock(_mutex);
    while (!buffer_.push_back(str))
    {
        _cv.wait(lock);
    }
    _cv.notify_all();

    return 1;
}

inline bool Collector::push(uint8_t *src, size_t size)
{
    if (get_size() <= size)
    {
        return 0;
    }

    std::unique_lock<std::mutex> lock(_mutex);
    while (!buffer_.push_back(src, size))
    {
        _cv.wait(lock);
    }
    _cv.notify_all();

    return 1;
}

inline size_t Collector::read(std::string &dest, int waitable_ms)
{
    size_t size;
    std::unique_lock<std::mutex> lock(_mutex);
    if (waitable_ms >= 0)
    {
        while (buffer_.is_empty())
        {
            if (waitable_ms == 0)
            {
                _cv.wait(lock);
            }
            else
            {
                if (std::cv_status::timeout ==
                    _cv.wait_for(lock, std::chrono::milliseconds(waitable_ms)))
                {
                    return 0;
                }
            }
        }
    }
    size = buffer_.pop_front(dest);
    _cv.notify_all();
    return size;
}

inline size_t Collector::read(uint8_t *dest, size_t dest_size, int waitable_ms)
{
    size_t size;
    std::unique_lock<std::mutex> lock(_mutex);
    if (waitable_ms >= 0)
    {
        while (buffer_.is_empty())
        {
            if (waitable_ms == 0)
            {
                _cv.wait(lock);
            }
            else
            {
                if (std::cv_status::timeout ==
                    _cv.wait_for(lock, std::chrono::milliseconds(waitable_ms)))
                {
                    return 0;
                }
            }
        }
    }
    size = buffer_.pop_front(dest, dest_size);
    _cv.notify_all();
    return size;
}

inline void Collector::handler()
{
    uint8_t buf[10000] = { 0 };
    size_t size = 0;
    while (true)
    {
        size = read(buf, sizeof(buf), READ_TIMEOUT_MS);
        if (size)
        {
            _call_back(buf, size);
        }
    }
}

inline bool Collector::start()
{
    if (_call_back != NULL)
    {
        std::thread thread(&Collector::handler, this);
        thread.detach();
        return true;
    }
    else
    {
        return false;
    }

}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


#endif // COLLECTOR_HPP
