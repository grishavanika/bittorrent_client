#include "torrent_client.h"
#include "utils_endian.h"
#include "asio_outcome_as_result.hpp"

#include <bencoding/be_torrent_file_parse.h>

#include <small_utils/utils_read_file.h>
#include <small_utils/utils_string.h>
#include <small_utils/utils_bytes.h>
#include <small_utils/utils_experimental.h>

#include <url.hpp>

#include <iostream>

#include <cstring>
#include <cassert>

namespace be
{
namespace cxxurl_detail
{ // from url.cpp
    static const uint8_t tbl[256] = {
        0,0,0,0, 0,0,0,0,     // NUL SOH STX ETX  EOT ENQ ACK BEL
        0,0,0,0, 0,0,0,0,     // BS  HT  LF  VT   FF  CR  SO  SI
        0,0,0,0, 0,0,0,0,     // DLE DC1 DC2 DC3  DC4 NAK SYN ETB
        0,0,0,0, 0,0,0,0,     // CAN EM  SUB ESC  FS  GS  RS  US
        0x00,0x01,0x00,0x00, 0x01,0x20,0x01,0x01, // SP ! " #  $ % & '
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x08, //  ( ) * +  , - . /
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  0 1 2 3  4 5 6 7
        0x01,0x01,0x04,0x01, 0x00,0x01,0x00,0x10, //  8 9 : ;  < = > ?
        0x02,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  @ A B C  D E F G
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  H I J K  L M N O
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  P Q R S  T U V W
        0x01,0x01,0x01,0x00, 0x00,0x00,0x00,0x01, //  X Y Z [  \ ] ^ _
        0x00,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  ` a b c  d e f g
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  h i j k  l m n o
        0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01, //  p q r s  t u v w
        0x01,0x01,0x01,0x00, 0x00,0x00,0x01,0x00, //  x y z {  | } ~ DEL
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
    };

    inline bool is_char(char c, std::uint8_t mask) {
        return (tbl[static_cast<unsigned char>(c)] & mask) != 0;
    }

    class encode_query_key {
    public:
        encode_query_key(const std::string& s, std::uint8_t mask) : m_s(s), m_mask(mask) {}
    private:
        const std::string& m_s;
        std::uint8_t m_mask;
        friend std::ostream& operator<< (std::ostream& o, const encode_query_key& e) {
            for (const char c : e.m_s)
                if (c == ' ')
                    o << '+';
                else if (c == '+')
                    o << "%2B";
                else if (c == '=')
                    o << "%3D";
                else if (c == '&')
                    o << "%26";
                else if (c == ';')
                    o << "%3B";
                else if (is_char(c, e.m_mask))
                    o << c;
                else
                    o << '%' << "0123456789ABCDEF"[((uint8_t)c) >> 4] << "0123456789ABCDEF"[((uint8_t)c) & 0xF];
            return o;
        }
    };


    class encode_query_val {
    public:
        encode_query_val(const std::string& s, std::uint8_t mask) : m_s(s), m_mask(mask) {}
    private:
        const std::string& m_s;
        std::uint8_t m_mask;
        friend std::ostream& operator<< (std::ostream& o, const encode_query_val& e) {
            for (const char c : e.m_s)
                if (c == ' ')
                    o << '+';
                else if (c == '+')
                    o << "%2B";
                else if (c == '&')
                    o << "%26";
                else if (c == ';')
                    o << "%3B";
                else if (is_char(c, e.m_mask))
                    o << c;
                else
                    o << '%' << "0123456789ABCDEF"[((uint8_t)c) >> 4] << "0123456789ABCDEF"[((uint8_t)c) & 0xF];
            return o;
        }
    };

    std::string build_query_str(const Url& url)
    {
        std::stringstream str;
        str << "?";
        auto it = url.query().begin(), end = url.query().end();
        if (it->key().empty())
            throw Url::build_error("First query entry has no key");
        str << encode_query_key(it->key(), 0x1F);
        if (!it->val().empty())
            str << "=" << encode_query_val(it->val(), 0x1F);
        while (++it != end) {
            if (it->key().empty())
                throw Url::build_error("A query entry has no key");
            str << "&" << encode_query_key(it->key(), 0x1F);
            if (!it->val().empty())
                str << "=" << encode_query_val(it->val(), 0x1F);
        }
        return std::move(str).str();
    }
} // cxxurl_detail

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
        return asio::ip::tcp::endpoint(asio::ip::address_v4(
              big_to_native(address.ipv4_))
            , big_to_native(address.port_));
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
        OUTCOME_TRY(TorrentFileInfo torrent, ParseTorrentFileContent(AsStringView(buffer)));

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

    auto TorrentClient::build_tracker_requests(const Tracker::RequestInfo& info) const
            -> outcome::result<Tracker::AllTrackers>
    {
        auto build_from_one = [this, &info]
            (const std::string& url_utf8)
                -> outcome::result<Tracker::Request>
        {
            // Url lib uses exceptions for errors. We want to
            // return nullopt.
            try
            {
                Url url(url_utf8);
                if (url.scheme() == "http")
                {
                    return build_http_request<Tracker::HTTP_GetRequest>(
                        url, info, 80/*default port*/);
                }
                else if (url.scheme() == "https")
                {
                    return build_http_request<Tracker::HTTPS_GetRequest>(
                        url, info, 443/*default port*/);
                }
                else if (url.scheme() == "udp")
                {
                    return build_udp_request(url, info);
                }
            } catch (...) {}
            return outcome::failure(ClientErrorc::TODO);
        };
        if (metainfo_.multi_trackers_.empty())
        {
            OUTCOME_TRY(Tracker::Request main, build_from_one(metainfo_.tracker_url_utf8_));
            return outcome::success(Tracker::AllTrackers(std::size_t(1), std::move(main)));
        }

        Tracker::AllTrackers all_trackers;
        all_trackers.reserve(metainfo_.multi_trackers_.size());
        for (const auto& tracker : metainfo_.multi_trackers_)
        {
            (void)tracker.tier_; // Ignore tier for now.
            if (auto data = build_from_one(tracker.url_utf8_))
            {
                all_trackers.push_back(std::move(data.value()));
            }
        }
        if (all_trackers.size() > 0)
        {
            return outcome::success(std::move(all_trackers));
        }
        return outcome::failure(ClientErrorc::TODO);
    }

    template<typename Body>
    auto TorrentClient::build_http_request(
        Url& url, const Tracker::RequestInfo& request, std::uint16_t default_port) const
            -> outcome::result<Tracker::Request>
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
        body.get_uri_ += cxxurl_detail::build_query_str(url);
        return outcome::success(Tracker::Request(std::move(body)));
    }

    auto TorrentClient::build_udp_request(
        Url& url, const Tracker::RequestInfo& info) const
            -> outcome::result<Tracker::Request>
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

        Tracker::UDP_Request udp;
        static_cast<Tracker::RequestInfo&>(udp) = info;
        udp.port_ = static_cast<std::uint16_t>(port);
        udp.host_ = url.host();
        udp.peer_id_ = peer_id_;
        udp.info_hash_ = info_hash_;
        return outcome::success(Tracker::Request(std::move(udp)));
    }

    static auto TryGetOnlyPeers(TrackerResponse&& response)
        -> outcome::result<std::vector<PeerAddress>>
    {
        using OkTag = TrackerResponse::OnSuccess;
        if (auto* peers = std::get_if<OkTag>(&response.data_))
        {
            return outcome::success(std::move(peers->peers_));
        }
        return outcome::failure(ClientErrorc::TODO);
    }

    static void RemoveDuplicates(std::vector<PeerAddress>& all_peers)
    {
        std::sort(std::begin(all_peers), std::end(all_peers)
            , [](const PeerAddress& lhs, const PeerAddress& rhs)
        {
            return (std::tie(lhs.ipv4_, lhs.port_) < std::tie(rhs.ipv4_, rhs.port_));
        });
        auto it = std::unique(std::begin(all_peers), std::end(all_peers)
            , [](const PeerAddress& lhs, const PeerAddress& rhs)
        {
            return (std::tie(lhs.ipv4_, lhs.port_) == std::tie(rhs.ipv4_, rhs.port_));
        });
        (void)all_peers.erase(it, std::end(all_peers));
    }

    co_asio_result<std::vector<PeerAddress>>
        TorrentClient::request_torrent_peers(asio::io_context& io_context
            , const Tracker::RequestInfo& info) const
    {
        auto fetch_one = [this, &io_context](Tracker::Request& data)
            -> co_asio_result<std::vector<PeerAddress>>
        {
            if (auto* http_get = std::get_if<Tracker::HTTP_GetRequest>(&data))
            {
                OUTCOME_CO_TRY(TrackerResponse response, co_await HTTP_TrackerAnnounce(io_context, *http_get));
                co_return TryGetOnlyPeers(std::move(response));
            }
            else if (auto* https_get = std::get_if<Tracker::HTTPS_GetRequest>(&data))
            {
                OUTCOME_CO_TRY(TrackerResponse response, co_await HTTPS_TrackerAnnounce(io_context, *https_get));
                co_return TryGetOnlyPeers(std::move(response));
            }
            else if (auto* udp = std::get_if<Tracker::UDP_Request>(&data))
            {
                OUTCOME_CO_TRY(TrackerResponse response, co_await UDP_TrackerAnnounce(io_context, *random_, *udp));
                co_return TryGetOnlyPeers(std::move(response));
            }
            co_return outcome::failure(ClientErrorc::TODO);
        };

        std::vector<PeerAddress> all_peers;
        auto try_add_peers = [&all_peers](outcome::result<std::vector<PeerAddress>>&& new_peers)
        {
            if (!new_peers)
            {
                return;
            }
            all_peers.insert(all_peers.end()
                , std::make_move_iterator(new_peers.value().begin())
                , std::make_move_iterator(new_peers.value().end()));
        };
        OUTCOME_CO_TRY(Tracker::AllTrackers all_infos, build_tracker_requests(info));
        for (Tracker::Request& data : all_infos)
        {
            try_add_peers(co_await fetch_one(data));
        }
        
        RemoveDuplicates(all_peers);

        if (all_peers.size() > 0)
        {
            co_return outcome::success(std::move(all_peers));
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

    co_asio_result<void> TorrentPeer::start(
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
        OUTCOME_CO_TRY(Message_Handshake parsed, Message_Handshake::ParseNetwork(response));
        if (!IsValidHandshakeResponse(parsed, info_hash))
        {
            co_return outcome::failure(ClientErrorc::TODO);
        }
        
        peer_id_ = parsed.peer_id_;
        extensions_ = parsed.reserved_;
        co_return outcome::success();
    }
} // namespace be
