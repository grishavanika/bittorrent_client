#include <small_utils/utils_read_file.h>

FileBuffer::FileBuffer(FileBuffer&& rhs) noexcept
    : data_(std::exchange(rhs.data_, nullptr))
    , size_(std::exchange(rhs.size_, 0))
{
}

FileBuffer::~FileBuffer() noexcept
{
    free(data_);
    data_ = nullptr;
    size_ = 0;
}

FileBuffer ReadAllFileAsBinary(const char* filepath)
{
#if (_MSC_VER)
    FILE* f = nullptr;
    const errno_t e = fopen_s(&f, filepath, "rb");
    (void)e;
#else
    FILE* const f = fopen(filepath, "rb");
#endif
    if (!f)
    {
        return {};
    }
    struct Scope { FILE* _f; ~Scope() { (void)fclose(_f); } } _{f};
    int status = fseek(f, 0, SEEK_END);
    if (status != 0)
    {
        return {};
    }
    const long fsize = ftell(f);
    if (fsize == -1L)
    {
        return {};
    }
    status = fseek(f, 0, SEEK_SET);
    if (status != 0)
    {
        return {};
    }
    const std::size_t size_to_read = static_cast<std::size_t>(fsize);
    FileBuffer buffer;
    buffer.data_ = malloc(size_to_read);
    buffer.size_ = size_to_read;
    if (!buffer.data_)
    {
        return {};
    }
    const std::size_t read = fread(buffer.data_, 1, size_to_read, f);
    if (read != size_to_read)
    {
        return {};
    }
    return buffer;
}
