#include "torrent_messages.h"
#include <small_utils/utils_string.h>

#include <asio/detail/socket_ops.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/write.hpp>
#include <asio/read.hpp>

#include <cstring>
#include <cassert>
#include <cstdint>

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
#  define coro(EC) \
    asio::redirect_error(asio::use_awaitable_t(__FILE__, __LINE__, __FUNCSIG__), ec)
#else
#  define coro(EC) \
    asio::redirect_error(asio::use_awaitable, ec)
#endif

namespace be
{
    namespace detail
    {
        // host_to_network_*
        // network_to_host_*
        using namespace asio::detail::socket_ops;

        std::uint32_t HostToNetworkOrder(std::uint32_t v)
        {
            return std::uint32_t(host_to_network_long(
                asio::detail::u_long_type(v)));
        }

        std::uint32_t NetworkToHostOrder(std::uint32_t v)
        {
            return std::uint32_t(network_to_host_long(
                asio::detail::u_long_type(v)));
        }
    } // namespace detail

    template<typename Message>
    static void EmplaceMessage(AnyMessage& m, std::vector<std::uint8_t>& data)
    {
        // Nothing should be constructed yet.
        assert(m.index() == 0);
        // We don't strip 1-byte PeerMessageId
        // to avoid vector reallocation.
        assert(data.size() >= 1);
        auto o = Message::FromBuffer(std::move(data));
        if (o)
        {
            m.emplace<Message>(std::move(*o));
        }
    }

    /*static*/ auto Message_Handshake::SerializeDefault(
        const SHA1Bytes& info_hash, const PeerId& client_id)
            -> Buffer
    {
        const ExtensionsBuffer reserved; // No extensions.

        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(std::uint8_t(SizeNoNull(k_protocol)))
            .write_str_no_null(k_protocol)
            .write(reserved.data_)
            .write(info_hash.data_)
            .write(client_id.data_)
            .finalize();
        return buffer;
    }

    /*static*/ std::optional<Message_Bitfield>
        Message_Bitfield::ParseNetwork(std::vector<std::uint8_t> payload)
    {
        std::optional<Message_Bitfield> o(std::in_place);
        o->data_ = std::move(payload);
        return o;
    }

    bool Message_Bitfield::has_piece(std::size_t index) const
    {
        // We store message id (1 byte at the beginning of the buffer).
        // Need to shift everything to the right, hence +1/-1 everywhere.
        if (data_.empty())
        {
            return false;
        }
        const std::size_t byte_index = (index / 8);
        const std::size_t offset = (index % 8);
        // Ignore 1-byte PeerMessageId.
        const std::size_t count = (data_.size() - 1);
        if (byte_index >= count)
        {
            return false;
        }
        // Shift to the right, skip 1-byte PeerMessageId.
        const std::uint8_t mask = data_[byte_index + 1];
        return (((mask >> (7u - offset)) & 1u) != 0u);
    }

    bool Message_Bitfield::set_piece(std::size_t index)
    {
        const std::size_t byte_index = (index / 8);
        const std::size_t offset = (index % 8);
        const std::size_t count = (data_.size() - 1);
        if (byte_index >= count)
        {
            return false;
        }
        // Shift to the right, skip 1-byte PeerMessageId.
        std::uint8_t& mask = data_[byte_index + 1];
        mask |= (1u << (7u - offset));
        return true;
    }

    /*static*/ std::optional<Message_Handshake>
        Message_Handshake::ParseNetwork(const Buffer& buffer)
    {
        std::optional<Message_Handshake> m(std::in_place);
        const bool ok = BytesReader::make(buffer.data_)
            .read(m->protocol_length_)
            .consume(m->pstr_, m->protocol_length_/*how many*/)
            .read(m->reserved_.data_)
            .read(m->info_hash_.data_)
            .read(m->peer_id_.data_)
            .finalize();
        return (ok ? m : std::nullopt);
    }

    /*static*/ std::optional<Message_Have>
        Message_Have::ParseNetwork(std::vector<std::uint8_t> payload)
    {
        PeerMessageId id{};
        std::uint32_t piece_index_network = 0;
        const bool ok = BytesReader::make(&payload[0], payload.size())
            .read(id)
            .read(piece_index_network)
            .finalize();
        if (!ok) { return std::nullopt; }
        std::optional<Message_Have> o(std::in_place);
        o->piece_index_ = detail::NetworkToHostOrder(piece_index_network);
        return o;
    }

    auto Message_Have::serialize() const -> Buffer
    {
        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(detail::HostToNetworkOrder(k_size))
            .write(PeerMessageId::Have)
            .write(detail::HostToNetworkOrder(piece_index_))
            .finalize();
        return buffer;
    }

    auto Message_Request::serialize() const -> Buffer
    {
        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(detail::HostToNetworkOrder(k_size))
            .write(PeerMessageId::Request)
            .write(detail::HostToNetworkOrder(piece_index_))
            .write(detail::HostToNetworkOrder(offset_))
            .write(detail::HostToNetworkOrder(length_))
            .finalize();
        return buffer;
    }

    std::optional<Message_Piece>
        Message_Piece::ParseNetwork(std::vector<std::uint8_t> payload)
    {
        PeerMessageId id{};
        std::uint32_t piece_index_network = 0;
        std::uint32_t begin_network = 0;
        auto reader = BytesReader::make(&payload[0], payload.size());
        const size_t piece_size = reader
            .read(id)
            .read(piece_index_network)
            .read(begin_network)
            .get_remaining();
        if ((piece_size == 0)
            || (piece_size == std::size_t(-1)))
        {
            return std::nullopt;
        }

        std::optional<Message_Piece> o(std::in_place);
        o->data_offset_ = (reader.current_ - &payload[0]);
        o->piece_index_ = detail::NetworkToHostOrder(piece_index_network);
        o->piece_begin_ = detail::NetworkToHostOrder(begin_network);
        o->payload_ = std::move(payload);
        return o;
    }

    std::uint32_t Message_Piece::size() const
    {
        assert(payload_.size() > data_offset_);
        return std::uint32_t(payload_.size() - data_offset_);
    }

    const void* Message_Piece::data() const
    {
        assert(data_offset_ < payload_.size());
        return &payload_[data_offset_];
    }

    asio::awaitable<AnyMessage> ReadAnyMessage(asio::ip::tcp::socket& peer)
    {
        std::error_code ec;

        std::uint32_t length = 0;
        (void)co_await asio::async_read(peer
            , asio::buffer(&length, sizeof(length)), coro(ec));
        if (ec) { co_return AnyMessage(); }
        length = detail::NetworkToHostOrder(length);
        if (length == 0)
        {
            AnyMessage m;
            m.emplace<Message_KeepAlive>();
            co_return m;
        }

        std::vector<std::uint8_t> data;
        data.resize(length);
        (void)co_await asio::async_read(peer
            , asio::buffer(&data[0], length), coro(ec));
        if (ec) { co_return AnyMessage(); }
        PeerMessageId message_id{};
        BytesReader::make(&data[0], length).read(message_id);
        
        AnyMessage m;
        switch (message_id)
        {
        case PeerMessageId::Choke:         EmplaceMessage<Message_Choke>(m, data);         break;
        case PeerMessageId::Unchoke:       EmplaceMessage<Message_Unchoke>(m, data);       break;
        case PeerMessageId::Interested:    EmplaceMessage<Message_Interested>(m, data);    break;
        case PeerMessageId::NotInterested: EmplaceMessage<Message_NotInterested>(m, data); break;
        case PeerMessageId::Have:          EmplaceMessage<Message_Have>(m, data);          break;
        case PeerMessageId::Bitfield:      EmplaceMessage<Message_Bitfield>(m, data);      break;
        case PeerMessageId::Request:       EmplaceMessage<Message_Request>(m, data);       break;
        case PeerMessageId::Piece:         EmplaceMessage<Message_Piece>(m, data);         break;
        case PeerMessageId::Cancel:        EmplaceMessage<Message_Cancel>(m, data);        break;
        default:                           EmplaceMessage<Message_Unknown>(m, data);       break;
        }
        co_return m;
    }

    asio::awaitable<bool> SendAnyMessage(asio::ip::tcp::socket& peer
        , const void* data, std::size_t size)
    {
        if (!data || (size == 0))
        {
            co_return false;
        }

        std::error_code ec;
        (void)co_await asio::async_write(peer
            , asio::buffer(data, size), coro(ec));
        if (ec) { co_return false; }
        co_return true;
    }

} // namespace be
