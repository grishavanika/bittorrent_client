#pragma once
#include <small_utils/utils_bytes.h>

#include <optional>

#include <cstdint>

namespace be
{
    struct Message_Handshake
    {
        static constexpr char k_protocol[] = "BitTorrent protocol";

        static constexpr std::size_t k_size =
              1 // 1 byte 'protocol_length_' = 0x13
            + (sizeof(k_protocol) - 1) // 19 bytes 'pstr_'
            + 8 // 8 bytes 'reserved_', extensions
            + sizeof(SHA1Bytes) // 20 bytes of 'info_hash_'
            + sizeof(PeerId); // 20 bytes of 'peer_id_'
        static_assert(k_size == 68);

        std::uint8_t protocol_length_ = 0;
        const char* pstr_ = nullptr;
        std::uint8_t reserved_[8]{};
        SHA1Bytes info_hash_;
        PeerId peer_id_;

        using Buffer = Buffer<k_size, Message_Handshake>;

        static Buffer SerializeDefault(
            const SHA1Bytes& info_hash, const PeerId& client_id);

        static std::optional<Message_Handshake> Parse(const Buffer& buffer);
    };

} // namespace be
