#include "torrent_client.h"
#include "utils_http.h"

#include <bencoding/be_torrent_file_parse.h>
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_string.h>
#include <small_utils/utils_bytes.h>

#include <url.hpp>

#include <cstring>

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

    /*static*/ std::optional<TorrentClient> TorrentClient::make(
        const char* torrent_file_path
        , std::random_device& random)
    {
        std::optional<TorrentClient> client(std::in_place);
        {
            const FileBuffer buffer = ReadAllFileAsBinary(torrent_file_path);
            if (!buffer.data_)
            {
                return std::nullopt;
            }
            std::optional<TorrentFileInfo> torrent = ParseTorrentFileContent(AsStringView(buffer));
            if (!torrent)
            {
                return std::nullopt;
            }

            client->metainfo_ = std::move(torrent->metainfo_);
            client->info_hash_ = GetSHA1(AsStringView(buffer
                , torrent->info_position_.start_
                , torrent->info_position_.end_));
        }

        client->peer_id_ = GetRandomPeerId(random);
        return client;
    }

    static std::optional<TorrentClient::HTTPTrackerRequest>
        TryBuildHTTPTrackerRequest(const Url& url)
    {
        std::optional<TorrentClient::HTTPTrackerRequest> request(std::in_place);
        request->host_ = url.host();
        if (request->host_.empty()
            // We support raw HTTP requests only for now.
            || url.scheme() != "http")
        {
            return std::nullopt;
        }
        request->port_ = 80;
        if (!url.port().empty())
        {
            std::uint64_t v = 0;
            if (!ParseLength(url.port(), v))
            {
                return std::nullopt;
            }
            request->port_ = static_cast<std::uint16_t>(v);
        }
        request->get_uri_ = url.path();
        if (request->get_uri_.empty())
        {
            request->get_uri_ += '/';
        }
        request->get_uri_ += url.query_str();
        return request;
    }

    auto TorrentClient::get_tracker_request_info(
        std::uint16_t server_port /*= 6882*/) const
            -> std::optional<HTTPTrackerRequest>
    {
        const std::size_t pieces = (metainfo_.info_.pieces_SHA1_.size() / sizeof(SHA1Bytes));
        const std::size_t uploaded_pieces = 0;
        const std::size_t downloaded_pieces = 0;
        const std::size_t do_compact_response = 1;

        // Url lib uses exceptions for errors. We want to
        // return nullopt.
        try
        {
            Url url(metainfo_.tracker_url_utf8_);
            url.add_query("info_hash",   AsString(info_hash_))
                .add_query("peer_id",    AsString(peer_id_))
                .add_query("port",       std::to_string(server_port))
                .add_query("uploaded",   std::to_string(uploaded_pieces))
                .add_query("downloaded", std::to_string(downloaded_pieces))
                .add_query("left",       std::to_string(pieces))
                .add_query("compact",    std::to_string(do_compact_response));
            return TryBuildHTTPTrackerRequest(url);
        }
        catch (...) { }
        return std::nullopt;
    }

    asio::awaitable<std::optional<be::TrackerResponse>>
        TorrentClient::request_torrent_peers(asio::io_context& io_context)
    {
        auto http = get_tracker_request_info();
        if (!http) { co_return std::nullopt; }
        std::optional<std::string> body = co_await HTTP_GET(
            io_context, http->host_, http->get_uri_, http->port_);
        if (!body) { co_return std::nullopt; }
        co_return be::ParseTrackerCompactResponseContent(*body);
    }

    asio::awaitable<std::optional<asio::ip::tcp::socket>>
        TorrentPeer::do_connect(PeerAddress address)
    {
        // network_to_*
        using namespace asio::detail::socket_ops;

        std::error_code ec;
        auto coro = asio::redirect_error(asio::use_awaitable, ec);

        const asio::ip::tcp::endpoint endpoint(asio::ip::address_v4(
            network_to_host_long(address.ipv4_))
            , network_to_host_short(address.port_));
        asio::ip::tcp::socket socket(*io_context_);
        co_await socket.async_connect(endpoint, coro);
        if (ec) { co_return std::nullopt; }
        co_return std::move(socket);
    }

    static bool IsValidHandshakeResponse(
        const Message_Handshake& response
        , const SHA1Bytes& info_hash)
    {
        return (response.protocol_length_ == SizeNoNull(Message_Handshake::k_protocol))
            && (std::memcmp(response.pstr_, Message_Handshake::k_protocol, response.protocol_length_) == 0)
            && (std::memcmp(response.info_hash_.data_, info_hash.data_, sizeof(info_hash.data_)) == 0);
    }

    asio::awaitable<std::optional<PeerInfo>>
        TorrentPeer::do_handshake(const SHA1Bytes& info_hash, const PeerId& peer_id)
    {
        if (!socket_) { co_return std::nullopt; }

        std::error_code ec;
        auto coro = asio::redirect_error(asio::use_awaitable, ec);

        const auto handshake = Message_Handshake::SerializeDefault(info_hash, peer_id);
        (void)co_await asio::async_write(*socket_
            , asio::buffer(handshake.data_, sizeof(handshake.data_)), coro);
        if (ec) { co_return std::nullopt; }
        Message_Handshake::Buffer response;
        (void)co_await asio::async_read(*socket_
            , asio::buffer(response.data_, sizeof(response.data_)), coro);
        if (ec) { co_return std::nullopt; }
        auto parsed = Message_Handshake::Parse(response);
        if (!parsed) { co_return std::nullopt; }
        if (!IsValidHandshakeResponse(*parsed, info_hash)) { co_return std::nullopt; }
        
        std::optional<PeerInfo> info(std::in_place);
        info->peer_id_ = parsed->peer_id_;
        info->extensions_ = parsed->reserved_;
        co_return info;
    }

} // namespace be
