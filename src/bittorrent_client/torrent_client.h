#pragma once
#include "torrent_messages.h"
#include "tracker_requests.h"
#include "client_errors.h"
#include "utils_asio.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_tracker_response_parse.h>

#include <small_utils/utils_bytes.h>

class Url;

namespace be
{
    struct TorrentPeer
    {
        explicit TorrentPeer(asio::io_context& io_context);

        // Connect & Handshake.
        co_asio_result<void> start(
            const PeerAddress& address
            , const SHA1Bytes& info_hash
            , const PeerId& peer_id);

        asio::io_context* io_context_ = nullptr;
        asio::ip::tcp::socket socket_;
        // Information about peer that we are connected to.
        PeerId peer_id_;
        ExtensionsBuffer extensions_;
        Message_Bitfield bitfield_;
        bool unchocked_ = false;
    };

    struct TorrentClient
    {
    public:
        std::random_device* random_ = nullptr;
        TorrentMetainfo metainfo_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;

    public:
        static outcome::result<TorrentClient> make(
            const char* torrent_file_path
            , std::random_device& random);

        std::uint32_t get_pieces_count() const;
        std::uint64_t get_total_size_bytes() const;
        std::uint32_t get_piece_size_bytes() const;

        outcome::result<Tracker::AllTrackers> build_tracker_requests(
            const Tracker::RequestInfo& info) const;

        co_asio_result<std::vector<PeerAddress>>
            request_torrent_peers(asio::io_context& io_context
                , const Tracker::RequestInfo& info) const;

    private:
        template<typename Body>
        outcome::result<Tracker::Request> build_http_request(
            Url& url, const Tracker::RequestInfo& request, std::uint16_t default_port) const;
        outcome::result<Tracker::Request> build_udp_request(
            Url& url, const Tracker::RequestInfo& request) const;
    };
} // namespace be
