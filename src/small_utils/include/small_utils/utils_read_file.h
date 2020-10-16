#include <ScopeGuard.h>

#include <string_view>

#include <cstdio>
#include <cstdlib>
#include <cassert>

struct FileBuffer
{
    void* data_ = nullptr;
    std::size_t size_ = 0;

    FileBuffer() noexcept = default;
    FileBuffer(const FileBuffer&) = delete;
    FileBuffer& operator=(const FileBuffer&) = delete;
    FileBuffer& operator=(FileBuffer&&) = delete;

    FileBuffer(FileBuffer&& rhs) noexcept;
    ~FileBuffer() noexcept;
};

FileBuffer ReadAllFileAsBinary(const char* filepath);

inline std::string_view AsStringView(const FileBuffer& buffer
    , std::size_t start, std::size_t end)
{
    assert(start <= end);
    assert(end <= buffer.size_);
    const auto ptr = (static_cast<const char*>(buffer.data_) + start);
    return std::string_view(ptr, (end - start));
}

inline std::string_view AsStringView(const FileBuffer& buffer)
{
    return std::string_view(static_cast<const char*>(buffer.data_), buffer.size_);
}
