#include "torrent_messages.h"
#include <small_utils/utils_string.h>

#include <asio/detail/socket_ops.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/read.hpp>

#include <cstring>
#include <cassert>
#include <cstdint>

namespace be
{
    // host_to_network_*
    // network_to_host_*
    using namespace asio::detail::socket_ops;

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

    /*static*/ std::optional<Message_Handshake> Message_Handshake::Parse(const Buffer& buffer)
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

    namespace detail
    {
        std::uint32_t SizeWithNetworkOrder(std::size_t size)
        {
            return std::uint32_t(host_to_network_long(
                asio::detail::u_long_type(size)));
        }
    }

    static std::uint32_t NetworkToHostOrder(std::uint32_t v)
    {
        return std::uint32_t(network_to_host_long(
            asio::detail::u_long_type(v)));
    }

    template<typename M>
    static void EmplaceMessage(AnyMessage& m, std::vector<std::uint8_t>& data)
    {
        assert(m.index() == 0);
        // We don't strip 1-byte PeerMessageId
        // to avoid vector reallocation.
        assert(data.size() >= 1);
        auto o = M::FromBuffer(std::move(data));
        if (o)
        {
            m.emplace<M>(std::move(*o));
        }
    }

    asio::awaitable<AnyMessage> ReadAnyMessage(asio::ip::tcp::socket& peer)
    {
        std::error_code ec;
        auto coro = asio::redirect_error(asio::use_awaitable, ec);

        std::uint32_t length = 0;
        (void)co_await asio::async_read(peer
            , asio::buffer(&length, sizeof(length)), coro);
        if (ec) { co_return AnyMessage(); }
        length = NetworkToHostOrder(length);
        if (length == 0)
        {
            AnyMessage m;
            m.emplace<Message_KeepAlive>();
            co_return m;
        }

        std::vector<std::uint8_t> data;
        data.resize(length);
        (void)co_await asio::async_read(peer
            , asio::buffer(&data[0], length), coro);
        if (ec) { co_return AnyMessage(); }
        PeerMessageId message_id{};
        BytesReader::make(&data[0], length).read(message_id);
        
        AnyMessage m;
        switch (message_id)
        {
        case PeerMessageId::Choke:         EmplaceMessage<Message_Choke>(m, data); break;
        case PeerMessageId::Unchoke:       EmplaceMessage<Message_Unchoke>(m, data); break;
        case PeerMessageId::Interested:    EmplaceMessage<Message_Interested>(m, data); break;
        case PeerMessageId::NotInterested: EmplaceMessage<Message_NotInterested>(m, data); break;
        case PeerMessageId::Have:          EmplaceMessage<Message_Have>(m, data); break;
        case PeerMessageId::Bitfield:      EmplaceMessage<Message_Bitfield>(m, data); break;
        case PeerMessageId::Request:       EmplaceMessage<Message_Request>(m, data); break;
        case PeerMessageId::Piece:         EmplaceMessage<Message_Piece>(m, data); break;
        case PeerMessageId::Cancel:        EmplaceMessage<Message_Cancel>(m, data); break;
        default:                           EmplaceMessage<Message_Unknown>(m, data); break;
        }
        co_return m;
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
}
