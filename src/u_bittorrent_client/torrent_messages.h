#pragma once
#include <small_utils/utils_bytes.h>

#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/redirect_error.hpp>

#include <optional>
#include <variant>

#include <cstdint>

namespace be
{
    namespace detail
    {
        std::uint32_t HostToNetworkOrder(std::uint32_t v);
        std::uint32_t NetworkToHostOrder(std::uint32_t v);
    } // namespace detail

    // https://www.bittorrent.org/beps/bep_0003.html
    enum class PeerMessageId : std::uint8_t
    {
        Choke         = 0, // no payload
        Unchoke       = 1, // no payload
        Interested    = 2, // no payload
        NotInterested = 3, // no payload
        Have          = 4,
        Bitfield      = 5,
        Request       = 6,
        Piece         = 7,
        Cancel        = 8,
    };

    using ExtensionsBuffer = Buffer<8, struct Extensions_>;

    struct Message_Handshake
    {
        static constexpr char k_protocol[] = "BitTorrent protocol";

        static constexpr std::size_t k_size =
              1                        // 1 byte 'protocol_length_' = 0x13
            + (sizeof(k_protocol) - 1) // 19 bytes 'pstr_'
            + sizeof(ExtensionsBuffer) // 8 bytes 'reserved_', extensions
            + sizeof(SHA1Bytes)        // 20 bytes of 'info_hash_'
            + sizeof(PeerId);          // 20 bytes of 'peer_id_'
        static_assert(k_size == 68);


        std::uint8_t protocol_length_ = 0;
        const char* pstr_ = nullptr;
        ExtensionsBuffer reserved_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;

        using Buffer = Buffer<k_size, Message_Handshake>;

        static Buffer SerializeDefault(
            const SHA1Bytes& info_hash, const PeerId& client_id);

        static std::optional<Message_Handshake> ParseNetwork(const Buffer& buffer);
    };

    template<typename Message, PeerMessageId Id>
    struct Message_Base
    {
        static constexpr std::size_t k_size_no_payload =
              sizeof(std::uint32_t)  // 4 bytes, length
            + sizeof(PeerMessageId); // 1 byte, id

        using BufferNoPayload = Buffer<k_size_no_payload, Message>;

        static std::optional<Message> FromBuffer(std::vector<std::uint8_t> payload)
        {
            // 1-byte PeerMessageId at the beginning.
            assert(payload.size() >= 1);
            return Message::ParseNetwork(std::move(payload));
        }

        static std::optional<Message> ParseNetwork(std::vector<std::uint8_t> payload)
        {
            (void)payload; // Ignore payload by default.
            return std::optional<Message>(std::in_place);
        }

        BufferNoPayload serialize() const
        {
            BufferNoPayload buffer;
            BytesWriter::make(buffer.data_)
                .write(detail::HostToNetworkOrder(k_size_no_payload))
                .write(std::uint8_t(Id))
                .finalize();
            return buffer;
        }
    };

    struct Message_KeepAlive     : Message_Base<Message_KeepAlive,     PeerMessageId(-1)> { };
    struct Message_Unknown       : Message_Base<Message_Unknown,       PeerMessageId(-2)> { };
    struct Message_Choke         : Message_Base<Message_Choke,         PeerMessageId::Choke> { };
    struct Message_Unchoke       : Message_Base<Message_Unchoke,       PeerMessageId::Unchoke> { };
    struct Message_Interested    : Message_Base<Message_Interested,    PeerMessageId::Interested> { };
    struct Message_NotInterested : Message_Base<Message_NotInterested, PeerMessageId::NotInterested> { };
    struct Message_Cancel        : Message_Base<Message_Cancel,        PeerMessageId::Cancel> { };

    struct Message_Have : Message_Base<Message_Have, PeerMessageId::Have>
    {
        std::uint32_t piece_index_ = 0;

        static std::optional<Message_Have> ParseNetwork(std::vector<std::uint8_t> payload);
    };

    struct Message_Bitfield : Message_Base<Message_Bitfield, PeerMessageId::Bitfield>
    {
        std::vector<std::uint8_t> data_;

        bool has_piece(std::size_t index) const;
        bool set_piece(std::size_t index);

        static std::optional<Message_Bitfield> ParseNetwork(std::vector<std::uint8_t> payload);
    };

    struct Message_Request : Message_Base<Message_Request, PeerMessageId::Request>
    {
        static constexpr std::size_t k_size =
              sizeof(std::uint32_t)  // 4 bytes, length
            + sizeof(PeerMessageId)  // 1 byte, id
            + sizeof(std::uint32_t)  // 4 bytes, index
            + sizeof(std::uint32_t)  // 4 bytes, offset
            + sizeof(std::uint32_t); // 4 bytes, length
        static_assert(k_size == 17);

        using Buffer = Buffer<k_size, Message_Request>;

        std::uint32_t piece_index_ = 0;
        std::uint32_t offset_ = 0;
        std::uint32_t length_ = 0;

        Buffer serialize() const;
    };

    struct Message_Piece : Message_Base<Message_Piece, PeerMessageId::Piece>
    {
        // Warning: not actual data.
        // Slice at `data_offset_`.
        std::vector<std::uint8_t> payload_;

        std::size_t data_offset_ = 0;
        std::uint32_t piece_index_ = 0;
        std::uint32_t piece_begin_ = 0;

        static std::optional<Message_Piece> ParseNetwork(std::vector<std::uint8_t> payload);
    };

    using AnyMessage = std::variant<std::monostate
        , Message_Choke
        , Message_Unchoke
        , Message_Interested
        , Message_NotInterested
        , Message_Have
        , Message_Bitfield
        , Message_Request
        , Message_Piece
        , Message_Cancel
        , Message_KeepAlive
        , Message_Unknown>;

    asio::awaitable<AnyMessage> ReadAnyMessage(asio::ip::tcp::socket& peer);
    asio::awaitable<bool> SendAnyMessage(asio::ip::tcp::socket& peer
        , const void* data, std::size_t size);

    template<typename Message>
    asio::awaitable<std::optional<Message>> ReadMessage(asio::ip::tcp::socket& peer)
    {
        AnyMessage m = co_await ReadAnyMessage(peer);
        if (Message* exact = std::get_if<Message>(&m))
        {
            co_return std::move(*exact);
        }
        co_return std::nullopt;
    }

    template<typename Message>
    asio::awaitable<bool> SendMessage(asio::ip::tcp::socket& peer, const Message& m)
    {
        const auto buffer = m.serialize();
        co_return co_await SendAnyMessage(peer, buffer.data_, sizeof(buffer.data_));
    }

} // namespace be
