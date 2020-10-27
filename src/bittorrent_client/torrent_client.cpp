#include "torrent_client.h"
#include "utils_http.h"
#include "asio_outcome_as_result.hpp"

#include <bencoding/be_torrent_file_parse.h>
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_string.h>
#include <small_utils/utils_bytes.h>
#include <small_utils/utils_experimental.h>

#include <url.hpp>

#include <cstring>
#include <cassert>

#include <iostream>

// #QQQ: temporary. Unify host <-> network conventions.
#define QQ_SWAP_U64(l)            \
    ( ( ((l) >> 56) & 0x00000000000000FFULL ) |         \
        ( ((l) >> 40) & 0x000000000000FF00ULL ) |       \
        ( ((l) >> 24) & 0x0000000000FF0000ULL ) |       \
        ( ((l) >>  8) & 0x00000000FF000000ULL ) |       \
        ( ((l) <<  8) & 0x000000FF00000000ULL ) |       \
        ( ((l) << 24) & 0x0000FF0000000000ULL ) |       \
        ( ((l) << 40) & 0x00FF000000000000ULL ) |       \
        ( ((l) << 56) & 0xFF00000000000000ULL ) )

namespace be
{
    static std::string AsString(const SHA1Bytes& sha1)
    {
        const void* data = sha1.data_;
        return std::string(static_cast<const char*>(data), sizeof(sha1.data_));
    }

    static std::string AsString(const PeerId& peer)
    {
        const void* data = peer.data_;
        return std::string(static_cast<const char*>(data), sizeof(peer.data_));
    }

    static asio::ip::tcp::endpoint AsEndpoint(const PeerAddress& address)
    {
        // network_to_*
        using namespace asio::detail::socket_ops;
        return asio::ip::tcp::endpoint(asio::ip::address_v4(
            network_to_host_long(address.ipv4_))
            , network_to_host_short(address.port_));
    }

    static bool IsValidHandshakeResponse(
        const Message_Handshake& response
        , const SHA1Bytes& info_hash)
    {
        return (response.protocol_length_ == SizeNoNull(Message_Handshake::k_protocol))
            && (std::memcmp(response.pstr_, Message_Handshake::k_protocol, response.protocol_length_) == 0)
            && (std::memcmp(response.info_hash_.data_, info_hash.data_, sizeof(info_hash.data_)) == 0);
    }

    /*explicit*/ TorrentPeer::TorrentPeer(asio::io_context& io_context)
        : io_context_(&io_context)
        , socket_(io_context)
        , peer_id_()
        , extensions_()
        , bitfield_()
        , unchocked_(false)
    {
    }

    /*static*/ outcome::result<TorrentClient> TorrentClient::make(
        const char* torrent_file_path
        , std::random_device& random)
    {
        const FileBuffer buffer = ReadAllFileAsBinary(torrent_file_path);
        if (!buffer.data_)
        {
            return outcome::failure(ClientErrorc::TODO);
        }
        OUTCOME_TRY(torrent, ParseTorrentFileContent(AsStringView(buffer)));

        TorrentClient client;
        client.metainfo_ = std::move(torrent.metainfo_);
        client.info_hash_ = GetSHA1(AsStringView(buffer
            , torrent.info_position_.start_
            , torrent.info_position_.end_));
        client.peer_id_ = GetRandomPeerId(random);
        client.random_ = &random;

        {   // Validate that total size and piece size together
            // with pieces count all do make sense together.
            const std::uint32_t pieces_count = client.get_pieces_count();
            const std::uint32_t piece_size = client.get_piece_size_bytes();
            const std::uint64_t total_size = client.get_total_size_bytes();
            assert(pieces_count > 0);
            assert(piece_size > 0);
            const std::uint64_t size_except_last_piece =
                (std::uint64_t(piece_size) * std::uint64_t((pieces_count - 1)));
            assert(size_except_last_piece < total_size);
            const std::uint64_t last = (total_size - size_except_last_piece);
            assert(last <= std::uint64_t(piece_size));
        }
        return client;
    }

    auto TorrentClient::get_tracker_request_info(const RequestInfo& request) const
            -> outcome::result<TrackerRequestInfo>
    {
        // Url lib uses exceptions for errors. We want to
        // return nullopt.
        try
        {
            Url url(metainfo_.tracker_url_utf8_);
            if (url.scheme() == "http")
            {
                return build_http_request_info<HTTP_GetRequest>(
                    url, request, 80/*default port*/);
            }
            else if (url.scheme() == "https")
            {
                return build_http_request_info<HTTPS_GetRequest>(
                    url, request, 443/*default port*/);
            }
            else if (url.scheme() == "udp")
            {
                return build_udp_request_info(url, request);
            }
        }
        catch (...) { }
        return outcome::failure(ClientErrorc::TODO);
    }

    template<typename Body>
    auto TorrentClient::build_http_request_info(
        Url& url, const RequestInfo& request, std::uint16_t default_port) const
            -> outcome::result<TrackerRequestInfo>
    {
        if (url.host().empty())
        {
            return outcome::failure(ClientErrorc::TODO);
        }

        const std::size_t do_compact_response = 1;
        url.add_query("info_hash",   AsString(info_hash_))
            .add_query("peer_id",    AsString(peer_id_))
            .add_query("port",       std::to_string(request.server_port))
            .add_query("uploaded",   std::to_string(request.uploaded_pieces))
            .add_query("downloaded", std::to_string(request.downloaded_pieces))
            .add_query("left",       std::to_string(request.pieces_left))
            .add_query("compact",    std::to_string(do_compact_response));

        Body body;
        body.host_ = url.host();
        body.port_ = default_port;
        body.get_uri_ = url.path();

        if (!url.port().empty())
        {
            std::uint64_t v = 0;
            if (!ParseLength(url.port(), v))
            {
                return outcome::failure(ClientErrorc::TODO);
            }
            body.port_ = static_cast<std::uint16_t>(v);
        }

        if (body.get_uri_.empty())
        {
            body.get_uri_ += '/';
        }
        body.get_uri_ += url.query_str();
        return outcome::success(TrackerRequestInfo(std::move(body)));
    }

    auto TorrentClient::build_udp_request_info(
        Url& url, const RequestInfo& request) const
            -> outcome::result<TrackerRequestInfo>
    {
        if (url.host().empty()
            || url.port().empty())
        {
            return outcome::failure(ClientErrorc::TODO);
        }
        std::uint64_t port = 0;
        if (!ParseLength(url.port(), port))
        {
            return outcome::failure(ClientErrorc::TODO);
        }

        UDP_Request udp;
        static_cast<RequestInfo&>(udp) = request;
        udp.port_ = static_cast<std::uint16_t>(port);
        udp.host_ = url.host();
        return outcome::success(TrackerRequestInfo(std::move(udp)));
    }

    asio::awaitable<outcome::result<be::TrackerResponse>>
        TorrentClient::request_torrent_peers(asio::io_context& io_context
            , const RequestInfo& request)
    {
        OUTCOME_CO_TRY(info, get_tracker_request_info(request));
        if (auto* http_get = std::get_if<HTTP_GetRequest>(&info))
        {
            OUTCOME_CO_TRY(body, co_await HTTP_GET(io_context
                , http_get->host_, http_get->get_uri_, http_get->port_));
            co_return be::ParseTrackerCompactResponseContent(body);
        }
        else if (auto* https_get = std::get_if<HTTPS_GetRequest>(&info))
        {
            OUTCOME_CO_TRY(body, co_await HTTPS_GET_NoVerification(io_context
                , https_get->host_, https_get->get_uri_, https_get->port_));
            co_return be::ParseTrackerCompactResponseContent(body);
        }
        else if (auto* udp = std::get_if<UDP_Request>(&info))
        {
            OUTCOME_CO_TRY(response, co_await do_udp_tracker_announce(io_context, *udp));
            co_return outcome::success(std::move(response));
        }
        co_return outcome::failure(ClientErrorc::TODO);
    }

    std::uint32_t TorrentClient::get_pieces_count() const
    {
        return std::uint32_t(metainfo_.info_.pieces_SHA1_.size() / sizeof(SHA1Bytes));
    }

    std::uint64_t TorrentClient::get_total_size_bytes() const
    {
        const TorrentMetainfo::LengthOrFiles& data = metainfo_.info_.length_or_files_;

        if (const std::uint64_t* single_file = std::get_if<std::uint64_t>(&data))
        {
            return *single_file;
        }
        else if (const auto* multi_files
            = std::get_if<std::vector<TorrentMetainfo::File>>(&data))
        {
            std::uint64_t total = 0;
            for (const TorrentMetainfo::File& f : *multi_files)
            {
                total += f.length_bytes_;
            }
            return total;
        }
        assert(false && "Invalid torrent metainfo.");
        return 0;
    }

    std::uint32_t TorrentClient::get_piece_size_bytes() const
    {
        return std::uint32_t(metainfo_.info_.piece_length_bytes_);
    }

    asio::awaitable<outcome::result<void>> TorrentPeer::start(
        const PeerAddress& address
        , const SHA1Bytes& info_hash
        , const PeerId& peer_id)
    {
        auto coro = as_result(asio::use_awaitable);
        OUTCOME_CO_TRY(co_await socket_.async_connect(AsEndpoint(address), coro));
        const auto handshake = Message_Handshake::SerializeDefault(info_hash, peer_id);
        OUTCOME_CO_TRY(co_await asio::async_write(socket_, asio::buffer(handshake.data_), coro));
        Message_Handshake::Buffer response;
        OUTCOME_CO_TRY(co_await asio::async_read(socket_, asio::buffer(response.data_), coro));
        OUTCOME_CO_TRY(parsed, Message_Handshake::ParseNetwork(response));
        if (!IsValidHandshakeResponse(parsed, info_hash))
        {
            co_return outcome::failure(ClientErrorc::TODO);
        }
        
        peer_id_ = parsed.peer_id_;
        extensions_ = parsed.reserved_;
        co_return outcome::success();
    }

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
            // WARNING: this assumes host is little-endian.
            // (i.e., usage of QQ_SWAP_U64()).
            Buffer buffer;
            BytesWriter::make(buffer.data_)
                .write(QQ_SWAP_U64(connection_id_))
                .write(be::detail::HostToNetworkOrder(action_))
                .write(be::detail::HostToNetworkOrder(transaction_id_))
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
            // WARNING: this assumes host is little-endian.
            // (i.e., usage of QQ_SWAP_U64()).
            m.action_ = be::detail::NetworkToHostOrder(m.action_);
            m.transaction_id_ = be::detail::NetworkToHostOrder(m.transaction_id_);
            m.connection_id_ = QQ_SWAP_U64(m.connection_id_);
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
                .write(QQ_SWAP_U64(connection_id_))
                .write(be::detail::HostToNetworkOrder(action_))
                .write(be::detail::HostToNetworkOrder(transaction_id_))
                .write(info_hash_.data_)
                .write(peer_id_.data_)
                .write(QQ_SWAP_U64(downloaded_))
                .write(QQ_SWAP_U64(left_))
                .write(QQ_SWAP_U64(uploaded_))
                .write(be::detail::HostToNetworkOrder(event_))
                .write(be::detail::HostToNetworkOrder(IP_))
                .write(be::detail::HostToNetworkOrder(key_))
                .write(be::detail::HostToNetworkOrder(num_want_))
                .write(asio::detail::socket_ops::network_to_host_short(port_))
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
            m.action_ = be::detail::NetworkToHostOrder(m.action_);
            m.transaction_id_ = be::detail::NetworkToHostOrder(m.transaction_id_);
            m.intervals_secs_ = be::detail::NetworkToHostOrder(m.intervals_secs_);
            m.leechers_ = be::detail::NetworkToHostOrder(m.leechers_);
            m.seeders_ = be::detail::NetworkToHostOrder(m.seeders_);
            
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
    static asio::awaitable<outcome::result<std::size_t>> AsyncReceiveWithTimeout(
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
    // Should be 8 = 64 hours.
    // Instead, retry 2 times only (~45 secs).
    unsigned k_retry_N_max = 1;

    static std::chrono::seconds RetryTimeout(unsigned n)
    {
        return std::chrono::seconds(15 * unsigned(std::pow(2, n)));
    }

    template<typename Endpoint>
    asio::awaitable<outcome::result<std::uint64_t>>
        AsyncConnect(asio::io_context& io_context
            , asio::ip::udp::socket& socket
            , Endpoint& endpoint
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
                OUTCOME_CO_TRY(response, Message_UDP_Connect::ParseNetwork(buffer));
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

    template<typename Endpoint>
    asio::awaitable<outcome::result<be::TrackerResponse>>
        AsyncAnnounce(asio::io_context& io_context
            , asio::ip::udp::socket& socket
            , Endpoint& endpoint
            , std::random_device& random
            , const TorrentClient::UDP_Request& request
            , std::uint64_t connection_id
            , const SHA1Bytes& info_hash
            , const PeerId& peer_id)
    {
        auto coro = as_result(asio::use_awaitable);

        Message_UDP_Announce msg;
        msg.connection_id_ = connection_id;
        msg.transaction_id_ = random();
        msg.info_hash_ = info_hash;
        msg.peer_id_ = peer_id;
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
        OUTCOME_CO_TRY(response, Message_UDP_Announce_Response::ParseNetwork(buffer, read.value()));
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
    asio::awaitable<outcome::result<be::TrackerResponse>>
        TorrentClient::do_udp_tracker_announce(
            asio::io_context& io_context, const UDP_Request& request)
    {
        auto coro = as_result(asio::use_awaitable);
        asio::ip::udp::socket socket(io_context);
        std::error_code ec;
        socket.open(asio::ip::udp::v4(), ec);
        if (ec) { co_return outcome::failure(ec); }

        asio::ip::udp::resolver resolver(io_context);
        const std::string port = std::to_string(request.port_);
        OUTCOME_CO_TRY(endpoints, co_await resolver.async_resolve(request.host_, port, coro));
        OUTCOME_CO_TRY(connection_id, co_await AsyncConnect(
            io_context, socket, *endpoints, *random_));
        assert(connection_id != 0);
        OUTCOME_CO_TRY(response, co_await AsyncAnnounce(io_context
            , socket
            , *endpoints
            , *random_
            , request
            , connection_id
            , info_hash_
            , peer_id_));
        co_return outcome::success(std::move(response));
    }

} // namespace be
