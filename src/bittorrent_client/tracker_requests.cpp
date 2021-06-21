#include "tracker_requests.h"
#include "torrent_messages.h"
#include "asio_outcome_as_result.hpp"
#include "utils_http.h"
#include "utils_endian.h"

namespace be
{
    struct Message_UDP_Connect
    {
        std::uint32_t action_ = 0; // connect
        std::uint32_t transaction_id_ = 0;
        std::uint64_t connection_id_ = 0x41727101980; // magic constant

        static constexpr std::size_t k_size =
            + sizeof(std::uint32_t)  // action
            + sizeof(std::uint32_t)  // transaction_id
            + sizeof(std::uint64_t); // connection_id
        static_assert(k_size == 16);

        using Buffer = Buffer<k_size, Message_UDP_Connect>;

        Buffer serialize() const
        {
            Buffer buffer;
            BytesWriter::make(buffer.data_)
                .write(native_to_big(connection_id_))
                .write(native_to_big(action_))
                .write(native_to_big(transaction_id_))
                .finalize();
            return buffer;
        }

        static outcome::result<Message_UDP_Connect>
            ParseNetwork(const Buffer& buffer)
        {
            Message_UDP_Connect m;
            const bool ok = BytesReader::make(buffer.data_)
                .read(m.action_)
                .read(m.transaction_id_)
                .read(m.connection_id_)
                .finalize();
            if (!ok)
            {
                return outcome::failure(ClientErrorc::TODO);
            }
            m.action_ = big_to_native(m.action_);
            m.transaction_id_ = big_to_native(m.transaction_id_);
            m.connection_id_ = big_to_native(m.connection_id_);
            return outcome::success(std::move(m));
        }
    };

    struct Message_UDP_Announce
    {
        std::uint64_t connection_id_ = 0;
        std::uint32_t action_ = 1;
        std::uint32_t transaction_id_ = 0;
        SHA1Bytes info_hash_;
        PeerId peer_id_;
        std::uint64_t downloaded_ = 0;
        std::uint64_t left_ = 0;
        std::uint64_t uploaded_ = 0;
        std::uint32_t event_ = 0; // none
        std::uint32_t IP_ = 0; // default
        std::uint32_t key_ = 0; // need to be randomized
        std::uint32_t num_want_ = std::uint32_t(-1); // default
        std::uint16_t port_ = 0;

        static constexpr std::size_t k_size =
               sizeof(std::uint64_t)  // connection_id
             + sizeof(std::uint32_t)  // action
             + sizeof(std::uint32_t)  // transaction_id
             + sizeof(SHA1Bytes)      // 20 bytes, info_hash
             + sizeof(PeerId)         // 20 bytes, peer_id
             + sizeof(std::uint64_t)  // downloaded
             + sizeof(std::uint64_t)  // left
             + sizeof(std::uint64_t)  // uploaded
             + sizeof(std::uint32_t)  // event
             + sizeof(std::uint32_t)  // IP
             + sizeof(std::uint32_t)  // key
             + sizeof(std::uint32_t)  // num_want
             + sizeof(std::uint16_t); // port
        static_assert(k_size == 98);

        using Buffer = Buffer<k_size, Message_UDP_Announce>;

        Buffer serialize() const
        {
            Buffer buffer;
            BytesWriter::make(buffer.data_)
                .write(native_to_big(connection_id_))
                .write(native_to_big(action_))
                .write(native_to_big(transaction_id_))
                .write(info_hash_.data_)
                .write(peer_id_.data_)
                .write(native_to_big(downloaded_))
                .write(native_to_big(left_))
                .write(native_to_big(uploaded_))
                .write(native_to_big(event_))
                .write(native_to_big(IP_))
                .write(native_to_big(key_))
                .write(native_to_big(num_want_))
                .write(native_to_big(port_))
                .finalize();
            return buffer;
        }
    };

    struct Message_UDP_Announce_Response
    {
        std::uint32_t action_ = 1;
        std::uint32_t transaction_id_ = 0;
        std::uint32_t intervals_secs_ = 0;
        std::uint32_t leechers_ = 0;
        std::uint32_t seeders_ = 0;

        std::vector<be::PeerAddress> peers_;

        static constexpr std::size_t k_header_size =
               sizeof(std::uint32_t)  // action
             + sizeof(std::uint32_t)  // transaction_id
             + sizeof(std::uint32_t)  // intervals_secs
             + sizeof(std::uint32_t)  // leechers
             + sizeof(std::uint32_t); // seeders
        static_assert(k_header_size == 20);
        
        static constexpr std::size_t k_max_peers = 128;
        static constexpr std::size_t k_address_size =
              sizeof(std::uint32_t)  // ipv4
            + sizeof(std::uint16_t); // port
        static constexpr std::size_t k_size = k_header_size + (k_max_peers * k_address_size);

        using Buffer = Buffer<k_size, Message_UDP_Announce_Response>;

        static outcome::result<Message_UDP_Announce_Response>
            ParseNetwork(const Buffer& buffer, std::size_t actually_read)
        {
            assert(actually_read >= k_header_size);
            Message_UDP_Announce_Response m;
            const bool ok = BytesReader::make(buffer.data_, k_header_size)
                .read(m.action_)
                .read(m.transaction_id_)
                .read(m.intervals_secs_)
                .read(m.leechers_)
                .read(m.seeders_)
                .finalize();
            if (!ok)
            {
                return outcome::failure(ClientErrorc::TODO);
            }
            m.action_         = big_to_native(m.action_);
            m.transaction_id_ = big_to_native(m.transaction_id_);
            m.intervals_secs_ = big_to_native(m.intervals_secs_);
            m.leechers_       = big_to_native(m.leechers_);
            m.seeders_        = big_to_native(m.seeders_);
            
            const std::size_t peers_bytes = (actually_read - k_header_size);
            if ((peers_bytes % k_address_size) != 0)
            {
                return outcome::failure(ClientErrorc::TODO);
            }
            m.peers_.reserve(peers_bytes / k_address_size);
            auto reader = BytesReader::make(buffer.data_ + k_header_size, peers_bytes);
            while (reader.valid_
                && (reader.get_remaining() > 0))
            {
                be::PeerAddress peer;
                reader.read(peer.ipv4_)
                    .read(peer.port_);
                if (reader.valid_)
                {
                    m.peers_.push_back(peer);
                }
            }
            return outcome::success(std::move(m));
        }
    };

    template<typename Buffer>
    static co_asio_result<std::size_t> AsyncReceiveWithTimeout(
        asio::io_context& io_context
        , asio::ip::udp::socket& socket
        , Buffer& buffer
        , std::chrono::seconds timeout_from_now
        , std::size_t read_minimum = sizeof(Buffer))
    {
        asio::steady_timer timeout(io_context);
        timeout.expires_after(timeout_from_now);
        // Make-up fake timer with much bigger wait time.
        // Will be reset only by async receive callback
        // notifying we are done.
        asio::steady_timer read_end(io_context);
        read_end.expires_after(std::chrono::hours(timeout_from_now.count()));
        
        auto coro = as_result(asio::use_awaitable);
        asio::ip::udp::endpoint sender_endpoint;
        std::error_code error;
        bool finished = false;
        std::size_t read_total = 0;
        socket.async_receive_from(
            asio::buffer(buffer.data_), sender_endpoint
            , [&](std::error_code ec, std::size_t read)
            {
                const bool was_canceled = (ec == asio::error::operation_aborted);
                if (!was_canceled)
                {
                    timeout.cancel();
                }
                error = ec;
                read_total = read;
                finished = true;
                read_end.cancel();
            });
        auto ok = co_await timeout.async_wait(coro);
        if (ok || (ok.error() != asio::error::operation_aborted))
        {
            socket.cancel();
        }
        if (!finished)
        {
            ok = co_await read_end.async_wait(coro);
            assert(!ok && (ok.error() == asio::error::operation_aborted));
        }
        if (error)
        {
            co_return outcome::failure(error);
        }
        if (read_total >= read_minimum)
        {
            co_return outcome::success(read_total);
        }
        co_return outcome::failure(ClientErrorc::TODO);
    }

    // http://www.bittorrent.org/beps/bep_0015.html
    // If a response is not received after 15 * 2 ^ n seconds,
    // the client should retransmit the request,
    // where n starts at 0 and is increased up to 8 (3840 seconds)
    // after every retransmission. Note that it is necessary to
    // rerequest a connection ID when it has expired.
    // 
    // Should be 8 = 64 hours (with standard 15 secs base).
    // Instead, try only once.
    unsigned k_retry_N_max = 0;
    // Should be 15 secs.
    unsigned k_wait_base_secs = 2;

    static std::chrono::seconds RetryTimeout(unsigned n)
    {
        return std::chrono::seconds(k_wait_base_secs * unsigned(std::pow(2, n)));
    }

    using UDPEndpoint = asio::ip::udp::resolver::results_type::value_type;

    co_asio_result<std::uint64_t>
        AsyncConnect(asio::io_context& io_context
            , asio::ip::udp::socket& socket
            , const UDPEndpoint& endpoint
            , std::random_device& random)
    {
        static_assert(std::is_same_v<decltype(random()), std::uint32_t>
            , "std::random_device can't generate std::uint32_t by default.");

        auto coro = as_result(asio::use_awaitable);
        for (unsigned i = 0; i <= k_retry_N_max; ++i)
        {
            const auto timeout_secs = RetryTimeout(i);
            Message_UDP_Connect msg;
            msg.transaction_id_ = random();
            auto connect = msg.serialize();
            OUTCOME_CO_TRY(co_await socket.async_send_to(
                asio::buffer(connect.data_), endpoint, coro));
            Message_UDP_Connect::Buffer buffer;
            auto ok = co_await AsyncReceiveWithTimeout(io_context
                , socket, buffer, timeout_secs);
            if (ok)
            {
                OUTCOME_CO_TRY(Message_UDP_Connect response, Message_UDP_Connect::ParseNetwork(buffer));
                assert(response.action_ == 0);
                assert(response.transaction_id_ == msg.transaction_id_);
                assert(response.connection_id_ != 0);
                co_return outcome::success(response.connection_id_);
            }
            if (!ok && (ok.error() != asio::error::operation_aborted))
            {
                co_return outcome::failure(ok.error());
            }
        }
        // Retries end.
        co_return outcome::failure(ClientErrorc::TODO);
    }

    co_asio_result<be::TrackerResponse>
        AsyncAnnounce(asio::io_context& io_context
            , asio::ip::udp::socket& socket
            , const UDPEndpoint& endpoint
            , std::random_device& random
            , const Tracker::UDP_Request& request
            , std::uint64_t connection_id)
    {
        auto coro = as_result(asio::use_awaitable);

        Message_UDP_Announce msg;
        msg.connection_id_ = connection_id;
        msg.transaction_id_ = random();
        msg.info_hash_ = request.info_hash_;
        msg.peer_id_ = request.peer_id_;
        msg.downloaded_ = request.downloaded_pieces;
        msg.left_ = request.pieces_left;
        msg.uploaded_ = request.uploaded_pieces;
        msg.key_ = random();
        msg.port_ = request.port_;

        auto announce = msg.serialize();
        OUTCOME_CO_TRY(co_await socket.async_send_to(
            asio::buffer(announce.data_), endpoint, coro));
        // #QQQ: implement retry logic.
        Message_UDP_Announce_Response::Buffer buffer;
        auto read = co_await AsyncReceiveWithTimeout(io_context
            , socket, buffer, RetryTimeout(0)
            , Message_UDP_Announce_Response::k_header_size);
        if (!read)
        {
            co_return outcome::failure(ClientErrorc::TODO);
        }
        OUTCOME_CO_TRY(auto response, Message_UDP_Announce_Response::ParseNetwork(buffer, read.value()));
        assert(response.action_ == 1);
        assert(response.transaction_id_ == msg.transaction_id_);

        be::TrackerResponse::OnSuccess tracker;
        tracker.rerequest_dt_secs_ = response.intervals_secs_;
        tracker.peers_ = std::move(response.peers_);
        if (tracker.peers_.size() > 0)
        {
            co_return outcome::success(be::TrackerResponse{std::move(tracker)});
        }
        co_return outcome::failure(ClientErrorc::TODO);
    }

    // http://www.bittorrent.org/beps/bep_0015.html
    co_asio_result<be::TrackerResponse>
        UDP_TrackerAnnounce(asio::io_context& io_context
            , std::random_device& random
            , const Tracker::UDP_Request& request)
    {
        auto coro = as_result(asio::use_awaitable);
        asio::ip::udp::socket socket(io_context);
        std::error_code ec;
        socket.open(asio::ip::udp::v4(), ec);
        if (ec) { co_return outcome::failure(ec); }

        asio::ip::udp::resolver resolver(io_context);
        const std::string port = std::to_string(request.port_);
        OUTCOME_CO_TRY(auto endpoints, co_await resolver.async_resolve(request.host_, port, coro));
        // Guarantees to be valid.
        auto& single_endpoint = *endpoints;

        OUTCOME_CO_TRY(std::uint64_t connection_id, co_await AsyncConnect(
            io_context, socket, single_endpoint, random));
        assert(connection_id != 0);
        OUTCOME_CO_TRY(be::TrackerResponse response, co_await AsyncAnnounce(io_context
            , socket
            , single_endpoint
            , random
            , request
            , connection_id));
        co_return outcome::success(std::move(response));
    }

    co_asio_result<TrackerResponse>
        HTTP_TrackerAnnounce(asio::io_context& io_context
            , const Tracker::HTTP_GetRequest& request)
    {
        OUTCOME_CO_TRY(std::string body, co_await HTTP_GET(io_context
            , request.host_, request.get_uri_, request.port_));
        co_return ParseTrackerCompactResponseContent(body);
    }

    co_asio_result<TrackerResponse>
        HTTPS_TrackerAnnounce(asio::io_context& io_context
            , const Tracker::HTTPS_GetRequest& request)
    {
        OUTCOME_CO_TRY(std::string body, co_await HTTPS_GET_NoVerification(io_context
            , request.host_, request.get_uri_, request.port_));
        co_return ParseTrackerCompactResponseContent(body);
    }

} // namespace be
