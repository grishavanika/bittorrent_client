#pragma once
#include <random>
#include <string_view> // std::span is not available in MinGW yet.
#include <type_traits>
#include <cstdint>
#include <cassert>

template<std::size_t N, typename>
struct Buffer
{
    std::uint8_t data_[N]{};
};

struct SHA1Bytes : Buffer<20, SHA1Bytes> { };
struct PeerId    : Buffer<20, PeerId> { };

SHA1Bytes GetSHA1(std::string_view data);
PeerId GetRandomPeerId(std::random_device& random);

struct BytesWriter
{
    std::uint8_t* current_ = nullptr;
    const std::uint8_t* end_ = nullptr;

    template<unsigned N>
    static BytesWriter make(std::uint8_t (&data)[N])
    {
        BytesWriter w;
        w.current_ = data;
        w.end_ = data + N;
        return w;
    }

    BytesWriter& write_data(const void* ptr, std::size_t size)
    {
        const std::size_t has = (end_ - current_);
        assert(has >= size);
        std::memcpy(current_, ptr, size);
        current_ += size;
        return *this;
    }

    void finalize() const
    {
        assert(current_ == end_);
    }

    template<typename T>
    BytesWriter& write(const T& v)
    {
        static_assert(std::is_trivial_v<T>);
        return write_data(&v, sizeof(v));
    }

    template<unsigned N>
    BytesWriter& write_str_no_null(const char (&str)[N])
    {
        return write_data(str, N - 1);
    }

    template<unsigned N>
    BytesWriter& write(const std::uint8_t (&data)[N])
    {
        return write_data(data, N);
    }
};

struct BytesReader
{
    const std::uint8_t* current_ = nullptr;
    const std::uint8_t* end_ = nullptr;
    bool valid_ = false;

    static BytesReader make(const void* ptr, std::size_t size)
    {
        BytesReader r;
        r.current_ = static_cast<const std::uint8_t*>(ptr);
        r.end_ = r.current_ + size;
        r.valid_ = (r.current_ != nullptr)
            && (r.current_ != r.end_);
        return r;
    }

    template<unsigned N>
    static BytesReader make(const std::uint8_t (&data)[N])
    {
        return make(data, N);
    }

    BytesReader& read_data(void* ptr, std::size_t size)
    {
        if (!valid_) { return *this; }
        const std::size_t has = (end_ - current_);
        if (has < size)
        {
            valid_ = false;
            return *this;
        }
        std::memcpy(ptr, current_, size);
        current_ += size;
        return *this;
    }

    bool finalize() const
    {
        return valid_ && (current_ == end_);
    }

    template<typename T>
    BytesReader& read(T& v)
    {
        static_assert(std::is_trivial_v<T>);
        return read_data(&v, sizeof(v));
    }

    template<unsigned N>
    BytesReader& read(std::uint8_t (&data)[N])
    {
        return read_data(data, N);
    }

    BytesReader& consume(const char*& ptr, std::size_t size)
    {
        if (!valid_) { return *this; }
        const std::size_t has = (end_ - current_);
        if (has < size)
        {
            valid_ = false;
            return *this;
        }
        ptr = reinterpret_cast<const char*>(current_);
        current_ += size;
        return *this;
    }

    std::size_t available() const
    {
        if (valid_)
        {
            return std::size_t(end_ - current_);
        }
        return std::size_t(-1);
    }
};
