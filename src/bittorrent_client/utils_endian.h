#pragma once
#include <cstdint>
#include <bit> // C++20 std::endian

// Small parts FROM Boost.Endian:
// https://github.com/boostorg/endian
namespace detail
{
    inline constexpr std::uint16_t endian_reverse(std::uint16_t x) noexcept
    {
        return (x << 8) | (x >> 8);
    }

    inline constexpr std::uint32_t endian_reverse(std::uint32_t x) noexcept
    {
        std::uint32_t step16 = x << 16 | x >> 16;
        return ((step16 << 8) & 0xff00ff00) | ((step16 >> 8) & 0x00ff00ff);
    }

    inline constexpr std::uint64_t endian_reverse(std::uint64_t x) noexcept
    {
        std::uint64_t step32 = x << 32 | x >> 32;
        std::uint64_t step16 = (step32 & 0x0000FFFF0000FFFFULL) << 16 | (step32 & 0xFFFF0000FFFF0000ULL) >> 16;
        return (step16 & 0x00FF00FF00FF00FFULL) << 8 | (step16 & 0xFF00FF00FF00FF00ULL) >> 8;
    }
} // namespace detail

template<typename T>
constexpr T native_to_big(T x)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return detail::endian_reverse(x);
    }
    else if constexpr (std::endian::native == std::endian::big)
    {
        return x;
    }
}

template<typename T>
constexpr T big_to_native(T x)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return detail::endian_reverse(x);
    }
    else if constexpr (std::endian::native == std::endian::big)
    {
        return x;
    }
}
