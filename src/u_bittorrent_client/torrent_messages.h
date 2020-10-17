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
    using ExtensionsBuffer = Buffer<8, struct Extensions_>;

    struct Message_Handshake
    {
        static constexpr char k_protocol[] = "BitTorrent protocol";

        static constexpr std::size_t k_size =
              1 // 1 byte 'protocol_length_' = 0x13
            + (sizeof(k_protocol) - 1) // 19 bytes 'pstr_'
            + sizeof(ExtensionsBuffer) // 8 bytes 'reserved_', extensions
            + sizeof(SHA1Bytes) // 20 bytes of 'info_hash_'
            + sizeof(PeerId); // 20 bytes of 'peer_id_'
        static_assert(k_size == 68);


        std::uint8_t protocol_length_ = 0;
        const char* pstr_ = nullptr;
        ExtensionsBuffer reserved_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;

        using Buffer = Buffer<k_size, Message_Handshake>;

        static Buffer SerializeDefault(
            const SHA1Bytes& info_hash, const PeerId& client_id);

        static std::optional<Message_Handshake> Parse(const Buffer& buffer);
    };

    // https://www.bittorrent.org/beps/bep_0003.html
    enum class PeerMessageId : std::uint8_t
    {
        Choke = 0, // no payload
        Unchoke = 1, // no payload
        Interested = 2, // no payload
        NotInterested = 3, // no payload
        Have = 4,
        Bitfield = 5,
        Request = 6,
        Piece = 7,
        Cancel = 8,
    };

    namespace detail
    {
        std::uint32_t SizeWithNetworkOrder(std::size_t size);
        std::uint32_t NetworkToHostOrder(std::uint32_t v);
    } // namespace detail

    template<PeerMessageId Id, typename Message>
    struct Message_Base
    {
        static std::optional<Message> FromBuffer(std::vector<std::uint8_t> data)
        {
            // 1-byte PeerMessageId at the beginning.
            assert(data.size() >= 1);
            return Message::Parse(std::move(data));
        }

    protected:
        static std::optional<Message> Parse(std::vector<std::uint8_t> data)
        {
            (void)data;
            return std::optional<Message>(std::in_place);
        }

        static constexpr std::size_t k_size_no_payload =
            sizeof(std::uint32_t) // 4 bytes, length
            + sizeof(PeerMessageId); // 1 byte, id

        using BufferNoPayload = Buffer<k_size_no_payload, Message>;

    public:
        BufferNoPayload serialize() const
        {
            BufferNoPayload buffer;
            BytesWriter::make(buffer.data_)
                .write(detail::SizeWithNetworkOrder(k_size_no_payload))
                .write(std::uint8_t(Id))
                .finalize();
            return buffer;
        }
    };

    struct Message_KeepAlive : Message_Base<PeerMessageId(-1), Message_KeepAlive> { };
    struct Message_Unknown : Message_Base<PeerMessageId(-2), Message_Unknown> { };

    struct Message_Choke : Message_Base<PeerMessageId::Choke, Message_Choke> { };
    struct Message_Unchoke : Message_Base<PeerMessageId::Unchoke, Message_Unchoke> { };
    struct Message_Interested : Message_Base<PeerMessageId::Interested, Message_Interested> { };
    struct Message_NotInterested : Message_Base<PeerMessageId::NotInterested, Message_NotInterested> { };

    struct Message_Have : Message_Base<PeerMessageId::Have, Message_Have>
    {
        std::uint32_t piece_index_ = 0;

        static std::optional<Message_Have> Parse(std::vector<std::uint8_t> data)
        {
            PeerMessageId id{};
            std::uint32_t piece_index_network = 0;
            const bool ok = BytesReader::make(&data[0], data.size())
                .read(id)
                .read(piece_index_network)
                .finalize();
            if (!ok) { return std::nullopt; }
            std::optional<Message_Have> o(std::in_place);
            o->piece_index_ = detail::NetworkToHostOrder(piece_index_network);
            return o;
        }
    };

    struct Message_Bitfield : Message_Base<PeerMessageId::Bitfield, Message_Bitfield>
    {
        std::vector<std::uint8_t> data_;

        static std::optional<Message_Bitfield> Parse(std::vector<std::uint8_t> data)
        {
            std::optional<Message_Bitfield> o(std::in_place);
            o->data_ = std::move(data);
            return o;
        }

        bool has_piece(std::size_t index) const;
        bool set_piece(std::size_t index);
    };

    struct Message_Request : Message_Base<PeerMessageId::Request, Message_Request>
    {
        std::uint32_t piece_index_ = 0;
        std::uint32_t offset_ = 0;
        std::uint32_t length_ = 0;

        static constexpr std::size_t k_size =
            sizeof(std::uint32_t) // 4 bytes, length
            + sizeof(PeerMessageId) // 1 byte, id
            + sizeof(std::uint32_t) // 4 bytes, index
            + sizeof(std::uint32_t) // 4 bytes, offset
            + sizeof(std::uint32_t); // 4 bytes, length
        static_assert(k_size == 17);

        using Buffer = Buffer<k_size, Message_Request>;

        Buffer serialize() const
        {
            Buffer buffer;
            BytesWriter::make(buffer.data_)
                .write(detail::SizeWithNetworkOrder(k_size))
                .write(PeerMessageId::Request)
                .write(detail::SizeWithNetworkOrder(piece_index_))
                .write(detail::SizeWithNetworkOrder(offset_))
                .write(detail::SizeWithNetworkOrder(length_))
                .finalize();
            return buffer;
        }
    };

    struct Message_Piece : Message_Base<PeerMessageId::Piece, Message_Piece>
    {
        // Warning: not actual data.
        std::vector<std::uint8_t> payload_;

        std::size_t data_offset_ = 0;
        std::uint32_t piece_index_ = 0;
        std::uint32_t piece_begin_ = 0;

        static std::optional<Message_Piece> Parse(std::vector<std::uint8_t> data)
        {
            PeerMessageId id{};
            std::uint32_t piece_index_network = 0;
            std::uint32_t begin_network = 0;
            auto reader = BytesReader::make(&data[0], data.size());
            const size_t piece_size = reader
                .read(id)
                .read(piece_index_network)
                .read(begin_network)
                .available();
            if ((piece_size == 0)
                || (piece_size == std::size_t(-1)))
            {
                return std::nullopt;
            }

            std::optional<Message_Piece> o(std::in_place);
            o->data_offset_ = (reader.current_ - &data[0]);
            o->piece_index_ = detail::NetworkToHostOrder(piece_index_network);
            o->piece_begin_ = detail::NetworkToHostOrder(begin_network);
            o->payload_ = std::move(data);
            return o;
        }
    };

    struct Message_Cancel : Message_Base<PeerMessageId::Cancel, Message_Cancel> { };

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
