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
// Кольцевой буффер, не thread safe.
// Хранит и выдает данные блоками.
class RingBuffer
{
private:
    size_t read_pos_;
    size_t write_pos_;
    std::vector<char> buffer_;

public:
    RingBuffer();
    RingBuffer(size_t size);

    long long read_pos() { return read_pos_; }
    long long write_pos() { return write_pos_; }
    bool can_read() { return read_pos_ != write_pos_; }

    void resize(size_t size);
    size_t size();
    bool write_block(std::string &str);
    bool write_block(uint8_t *s, size_t size);
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

inline bool RingBuffer::write_block(std::string &str)
{
    size_t size = str.size();
    size_t cur_pos = write_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        if (cur_pos == buffer_.size() - 1)
        {
            if (read_pos_ == 0)
            {
                return false;
            }
        }
        else
        {
            if (cur_pos == buffer_.size())
            {
                cur_pos = 0;
            }

            if (cur_pos == read_pos_ - 1)
            {
                return false;
            }
        }
        buffer_[cur_pos] = ((size >> (i * 8)) & 0XFF);
        cur_pos++;
    }

    for (size_t i = 0; i < size; ++i)
    {
        if (cur_pos == buffer_.size() - 1)
        {
            if (read_pos_ == 0)
            {
                return false;
            }
        }
        else
        {
            if (cur_pos == buffer_.size())
            {
                cur_pos = 0;
            }

            if (cur_pos == read_pos_ - 1)
            {
                return false;
            }
        }
        buffer_[cur_pos] = str[i];
        cur_pos++;
    }
    write_pos_ = cur_pos;
    return true;
}

inline bool RingBuffer::write_block(uint8_t *s, size_t size)
{
    size_t cur_pos = write_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        if (cur_pos == buffer_.size() - 1)
        {
            if (read_pos_ == 0)
            {
                return false;
            }
        }
        else
        {
            if (cur_pos == buffer_.size())
            {
                cur_pos = 0;
            }

            if (cur_pos == read_pos_ - 1)
            {
                return false;
            }
        }
        buffer_[cur_pos] = ((size >> (i * 8)) & 0XFF);
        cur_pos++;
    }

    for (size_t i = 0; i < size; ++i)
    {
        if (cur_pos == buffer_.size() - 1)
        {
            if (read_pos_ == 0)
            {
                return false;
            }
        }
        else
        {
            if (cur_pos == buffer_.size())
                cur_pos = 0;

            if (cur_pos == read_pos_ - 1)
                return false;
        }
        buffer_[cur_pos] = static_cast<char>(s[i]);
        cur_pos++;
    }
    write_pos_ = cur_pos;
    return true;
}

// Return read block size.
// Return block size in dest_size, if dest_size too small.
inline size_t RingBuffer::pop_front(uint8_t *dest, size_t &dest_size)
{
    if (read_pos_ == write_pos_)
        return 0;

    size_t size_block = 0;
    size_t cur_pos = read_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        size_block += (buffer_[cur_pos] << (8 * i));
        ++cur_pos;

        if (cur_pos == buffer_.size())
            cur_pos = 0;
    }

    size_t res_size = 0;
    size_t size_to_buffer_end = (cur_pos = 0 ? 0 : -1);
    for (size_t i = 0; i < size_block; ++i)
    {
        ++cur_pos;
        ++res_size;

        if (cur_pos == buffer_.size())
        {
            size_to_buffer_end = res_size;
            cur_pos = 0;
        }
    }

    if (dest_size < res_size)
    {
        dest_size = res_size;
        return 0;
    }
    else
    {
        if (size_to_buffer_end == -1)
        {
            // Read pos -> end
            memcpy(dest, buffer_.data() + read_pos_ + BYTES_SIZE, res_size);
        }
        else if (size_to_buffer_end == 0)
        {
            // Zero pos -> end
            memcpy(dest, buffer_.data(), res_size);
        }
        else
        {
            // Read pos -> end buffer; zero pos -> end;
            memcpy(dest, buffer_.data() + read_pos_ + BYTES_SIZE, size_to_buffer_end);
            memcpy(dest, buffer_.data(), res_size - size_to_buffer_end);
        }
    }

    read_pos_ = cur_pos;
    return res_size;
}


// Return read block size.
inline size_t RingBuffer::pop_front(std::string &dest)
{
    size_t size_block = 0;
    size_t cur_pos = read_pos_;
    for (int i = 0; i < BYTES_SIZE; ++i)
    {
        size_block += (buffer_[cur_pos] << (8 * i));
        ++cur_pos;

        if (cur_pos == buffer_.size())
            cur_pos = 0;
    }

    size_t res_size = 0;
    size_t size_to_buffer_end = (cur_pos = 0 ? 0 : -1);
    for (size_t i = 0; i < size_block; ++i)
    {
        ++cur_pos;
        ++res_size;

        if (cur_pos == buffer_.size())
        {
            size_to_buffer_end = res_size;
            cur_pos = 0;
        }
    }

    dest.reserve(res_size);
    if (size_to_buffer_end == -1)
    {
        // Read pos -> end
        dest.append(buffer_.data() + read_pos_ + BYTES_SIZE, res_size);
    }
    else if (size_to_buffer_end == 0)
    {
        // Zero pos -> end
        dest.append(buffer_.data(), res_size);
    }
    else
    {
        // Read pos -> end buffer; zero pos -> end;
        dest.append(buffer_.data() + read_pos_ + BYTES_SIZE, size_to_buffer_end);
        dest.append(buffer_.data(), res_size - size_to_buffer_end);
    }

    read_pos_ = cur_pos;
    return res_size;
}
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++






// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Collector - обертка, которая управляет кольцевым буфером.
// Thread safe, за исключением set_size() и set_callback(),
// предполагается однократный вызов этих функций на этапе инициализации.
#define READ_TIMEOUT_MS 3000
class Collector
{
private:
    std::mutex _mutex;
    std::condition_variable _cv;
    RingBuffer buffer_;
    collector_callback _call_back;

    size_t get_size();
    void handler();
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
    while (!buffer_.write_block(str))
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
    while (!buffer_.write_block(src, size))
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
        while (!buffer_.can_read())
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
        while (!buffer_.can_read())
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
