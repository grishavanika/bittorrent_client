#include <small_utils/utils_bytes.h>

#include <TinySHA1.hpp>

#include <cstring>

SHA1Bytes GetSHA1(std::string_view data)
{
    SHA1Bytes bytes{};
    if (data.empty())
    {
        return bytes;
    }
    sha1::SHA1 sha1;
    sha1.processBytes(data.data(), data.size())
        .getDigestBytes(bytes.data_);
    return bytes;
}

PeerId GetRandomPeerId(std::random_device& random)
{
    PeerId peer;
    std::uint8_t* current = peer.data_;
    const std::uint8_t* const end = peer.data_ + sizeof(peer.data_);
    while (current != end)
    {
        unsigned int n = random();
        const std::size_t available = std::min<std::size_t>(sizeof(n), (end - current));
        std::memcpy(current, &n, available);
        current += available;
    }
    return peer;
}
