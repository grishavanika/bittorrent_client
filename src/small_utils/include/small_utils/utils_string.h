#pragma once
#include <string_view>
#include <cstdint>

#if __has_include(<charconv>)
#  include <charconv>
#  define BE_HAS_FROM_CHARS() 1
#else
#  include <cstdlib>
#  define BE_HAS_FROM_CHARS() 0
#endif

inline bool ParseLength(std::string_view str, std::uint64_t& length)
{
#if (BE_HAS_FROM_CHARS())
    const char* begin = str.data();
    const char* end = begin + str.size();
    const auto result = std::from_chars(begin, end, length, 10);
    if ((result.ptr != end) || (result.ec != std::errc()))
    {
        return false;
    }
    return true;
#else
    // https://stackoverflow.com/questions/43787672/the-max-number-of-digits-in-an-int-based-on-number-of-bits
    static_assert(CHAR_BIT == 8
        , "Max decimal digits formula assumes byte is 8 bits");
    constexpr std::size_t k_buf_size = (241 * sizeof(length) / 100 + 1);

    const std::size_t size = str.size();
    if ((size == 0) || (size >= k_buf_size))
    {
        return false;
    }
    char temp[k_buf_size + 1];
#if defined(_MSC_VER)
    (void)strncpy_s(temp, str.data(), size);
#else
    (void)strncpy(temp, str.data(), size);
#endif
    temp[size] = '\0';
    const char* begin = temp;
    const char* end = begin + size;
    char* parse_end = nullptr;
    const unsigned long long v = std::strtoull(begin, &parse_end, 10);
    if ((begin != end) && (parse_end == end))
    {
        length = v;
        return true;
    }
    return false;
#endif
}

#undef BE_HAS_FROM_CHARS
