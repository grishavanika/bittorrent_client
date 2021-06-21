#include "torrent_messages.h"

#include <small_utils/utils_string.h>

#include <cstring>
#include <cassert>
#include <cstdint>

namespace be
{
    template<typename Message>
    static outcome::result<AnyMessage> MakeMessage(std::vector<std::uint8_t>& data)
    {
        // We don't strip 1-byte PeerMessageId
        // to avoid vector reallocation.
        assert(data.size() >= 1);
        OUTCOME_TRY(Message m, Message::FromBuffer(std::move(data)));
        return outcome::success(AnyMessage(std::move(m)));
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

    /*static*/ outcome::result<Message_Bitfield>
        Message_Bitfield::ParseNetwork(std::vector<std::uint8_t> payload)
    {
        Message_Bitfield m;
        m.data_ = std::move(payload);
        return outcome::success(std::move(m));
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

    /*static*/ outcome::result<Message_Handshake>
        Message_Handshake::ParseNetwork(const Buffer& buffer)
    {
        Message_Handshake m;
        const bool ok = BytesReader::make(buffer.data_)
            .read(m.protocol_length_)
            .consume(m.pstr_, m.protocol_length_/*how many*/)
            .read(m.reserved_.data_)
            .read(m.info_hash_.data_)
            .read(m.peer_id_.data_)
            .finalize();
        if (ok)
        {
            return outcome::success(std::move(m));
        }
        return outcome::failure(ClientErrorc::TODO);
    }

    /*static*/ outcome::result<Message_Have>
        Message_Have::ParseNetwork(std::vector<std::uint8_t> payload)
    {
        PeerMessageId id{};
        std::uint32_t piece_index_network = 0;
        const bool ok = BytesReader::make(&payload[0], payload.size())
            .read(id)
            .read(piece_index_network)
            .finalize();
        if (ok)
        {
            Message_Have m;
            m.piece_index_ = big_to_native(piece_index_network);
            return outcome::success(std::move(m));
        }
        return outcome::failure(ClientErrorc::TODO);
    }

    auto Message_Have::serialize() const -> Buffer
    {
        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(native_to_big(k_size))
            .write(PeerMessageId::Have)
            .write(native_to_big(piece_index_))
            .finalize();
        return buffer;
    }

    auto Message_Request::serialize() const -> Buffer
    {
        Buffer buffer;
        BytesWriter::make(buffer.data_)
            .write(native_to_big(k_size))
            .write(PeerMessageId::Request)
            .write(native_to_big(piece_index_))
            .write(native_to_big(offset_))
            .write(native_to_big(length_))
            .finalize();
        return buffer;
    }

    outcome::result<Message_Piece>
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
            return outcome::failure(ClientErrorc::TODO);
        }

        Message_Piece m;
        m.data_offset_ = (reader.current_ - &payload[0]);
        m.piece_index_ = big_to_native(piece_index_network);
        m.piece_begin_ = big_to_native(begin_network);
        m.payload_ = std::move(payload);
        return outcome::success(std::move(m));
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

    co_asio_result<AnyMessage> ReadAnyMessage(asio::ip::tcp::socket& peer)
    {
        auto coro = as_result(asio::use_awaitable);
        std::uint32_t length = 0;
        OUTCOME_CO_TRY(co_await asio::async_read(peer
            , asio::buffer(&length, sizeof(length)), coro));
        length = big_to_native(length);
        if (length == 0)
        {
            co_return outcome::success(AnyMessage(Message_KeepAlive{}));
        }

        std::vector<std::uint8_t> data;
        data.resize(length);
        OUTCOME_CO_TRY(co_await asio::async_read(peer
            , asio::buffer(&data[0], length), coro));
        PeerMessageId message_id{};
        BytesReader::make(&data[0], length).read(message_id);

        switch (message_id)
        {
        case PeerMessageId::Choke:         co_return MakeMessage<Message_Choke>(data);
        case PeerMessageId::Unchoke:       co_return MakeMessage<Message_Unchoke>(data);
        case PeerMessageId::Interested:    co_return MakeMessage<Message_Interested>(data);
        case PeerMessageId::NotInterested: co_return MakeMessage<Message_NotInterested>(data);
        case PeerMessageId::Have:          co_return MakeMessage<Message_Have>(data);
        case PeerMessageId::Bitfield:      co_return MakeMessage<Message_Bitfield>(data);
        case PeerMessageId::Request:       co_return MakeMessage<Message_Request>(data);
        case PeerMessageId::Piece:         co_return MakeMessage<Message_Piece>(data);
        case PeerMessageId::Cancel:        co_return MakeMessage<Message_Cancel>(data);
        default:                           co_return MakeMessage<Message_Unknown>(data);
        }
        co_return outcome::failure(ClientErrorc::TODO);
    }

} // namespace be
