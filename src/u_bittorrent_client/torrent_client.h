#pragma once
#include "torrent_messages.h"
#include "client_errors.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_tracker_response_parse.h>
#include <small_utils/utils_bytes.h>

#include <asio.hpp>

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
        explicit TorrentPeer(asio::io_context& io_context);

        // Connect & Handshake.
        asio::awaitable<outcome::result<void>> start(
            const PeerAddress& address
            , const SHA1Bytes& info_hash
            , const PeerId& peer_id);

        asio::io_context* io_context_ = nullptr;
        asio::ip::tcp::socket socket_;
        // Information about peer that we are connected to.
        PeerInfo info_;
        Message_Bitfield bitfield_;
        bool unchocked_ = false;
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

        std::uint32_t get_pieces_count() const;
        std::uint64_t get_total_size_bytes() const;
        std::uint32_t get_piece_size_bytes() const;

    public:
        TorrentMetainfo metainfo_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;
    };
} // namespace be
