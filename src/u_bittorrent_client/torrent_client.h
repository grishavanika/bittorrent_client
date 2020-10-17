#pragma once
#include "torrent_messages.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_tracker_response_parse.h>
#include <small_utils/utils_bytes.h>

#include <asio/io_context.hpp>
#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>

#include <optional>

namespace be
{
    struct PeerInfo
    {
        PeerId peer_id_;
        ExtensionsBuffer extensions_;
    };

    struct TorrentPeer
    {
        // (1)
        asio::awaitable<std::optional<asio::ip::tcp::socket>>
            do_connect(PeerAddress address);
        // (2)
        asio::awaitable<std::optional<PeerInfo>>
            do_handshake(const SHA1Bytes& info_hash, const PeerId& peer_id);
        // (3)
        asio::awaitable<std::optional<Message_Bitfield>>
            do_read_bitfield();

        asio::io_context* io_context_ = nullptr;
        // std::optional<> to default-construct.
        std::optional<asio::ip::tcp::socket> socket_;
        // Information about peer that we are connected to.
        std::optional<PeerInfo> info_;
        std::optional<Message_Bitfield> bitfield_;
    };

    struct TorrentClient
    {
        static std::optional<TorrentClient> make(
            const char* torrent_file_path
            , std::random_device& random);

        struct HTTPTrackerRequest
        {
            std::string host_;
            std::uint16_t port_ = 0;
            std::string get_uri_;
        };

        std::optional<HTTPTrackerRequest> get_tracker_request_info(
            std::uint16_t server_port = 6882) const;

        asio::awaitable<std::optional<be::TrackerResponse>>
            request_torrent_peers(asio::io_context& io_context);

    public:
        TorrentMetainfo metainfo_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;
    };
} // namespace be
