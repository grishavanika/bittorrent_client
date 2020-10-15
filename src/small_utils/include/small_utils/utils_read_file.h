#include <ScopeGuard.h>

#include <cstdio>
#include <cstdlib>

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
