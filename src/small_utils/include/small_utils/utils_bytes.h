#pragma once
#include <random>
#include <string_view> // std::span is not available in MinGW yet.
#include <cstdint>

struct SHA1Bytes
{
    std::uint8_t data_[20]{};
};

struct PeerId
{
    std::uint8_t data_[20]{};
};

SHA1Bytes GetSHA1(std::string_view data);
PeerId GetRandomPeerId(std::random_device& random);
