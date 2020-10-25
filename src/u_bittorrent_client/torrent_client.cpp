#include "torrent_client.h"
#include "utils_http.h"
#include "as_result.hpp"

#include <bencoding/be_torrent_file_parse.h>
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_string.h>
#include <small_utils/utils_bytes.h>

#include <url.hpp>

#include <cstring>
#include <cassert>

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

    static outcome::result<TorrentClient::HTTPTrackerRequest>
        TryBuildHTTPTrackerRequest(const Url& url)
    {
        const bool allowed_scheme = ((url.scheme() == "http") || (url.scheme() == "https"));
        if (url.host().empty() || !allowed_scheme)
        {
            return outcome::failure(ClientErrorc::TODO);
        }

        TorrentClient::HTTPTrackerRequest request;
        request.host_ = url.host();
        request.use_https_ = (url.scheme() == "https");
        request.port_ = (request.use_https_ ? 443 : 80);
        if (!url.port().empty())
        {
            std::uint64_t v = 0;
            if (!ParseLength(url.port(), v))
            {
                return outcome::failure(ClientErrorc::TODO);
            }
            request.port_ = static_cast<std::uint16_t>(v);
        }
        request.get_uri_ = url.path();
        if (request.get_uri_.empty())
        {
            request.get_uri_ += '/';
        }
        request.get_uri_ += url.query_str();
        return outcome::success(std::move(request));
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
            -> outcome::result<HTTPTrackerRequest>
    {
        const std::size_t do_compact_response = 1;
        // Url lib uses exceptions for errors. We want to
        // return nullopt.
        try
        {
            Url url(metainfo_.tracker_url_utf8_);
            url.add_query("info_hash",   AsString(info_hash_))
                .add_query("peer_id",    AsString(peer_id_))
                .add_query("port",       std::to_string(request.server_port))
                .add_query("uploaded",   std::to_string(request.uploaded_pieces))
                .add_query("downloaded", std::to_string(request.downloaded_pieces))
                .add_query("left",       std::to_string(request.pieces_count))
                .add_query("compact",    std::to_string(do_compact_response));
            return TryBuildHTTPTrackerRequest(url);
        }
        catch (...) { }
        return outcome::failure(ClientErrorc::TODO);
    }

    asio::awaitable<outcome::result<be::TrackerResponse>>
        TorrentClient::request_torrent_peers(asio::io_context& io_context
            , const RequestInfo& request)
    {
        OUTCOME_CO_TRY(http, get_tracker_request_info(request));
        outcome::result<std::string> body = http.use_https_
            ? co_await HTTPS_GET_NoVerification(io_context
                , http.host_, http.get_uri_, http.port_)
            : co_await HTTP_GET(io_context
                , http.host_, http.get_uri_, http.port_);
        OUTCOME_CO_TRY(body);
        co_return be::ParseTrackerCompactResponseContent(body.value());
    }

    std::uint32_t TorrentClient::get_pieces_count() const
    {
        return std::uint32_t(metainfo_.info_.pieces_SHA1_.size() / sizeof(SHA1Bytes));
    }

    std::uint64_t TorrentClient::get_total_size_bytes() const
    {
        const TorrentMetainfo::LengthOrFiles& data = metainfo_.info_.length_or_files_;
        assert(data.index() != 0);

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

} // namespace be
