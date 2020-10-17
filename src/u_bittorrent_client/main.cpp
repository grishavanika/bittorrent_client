#include "torrent_client.h"
#include "torrent_messages.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_tracker_response_parse.h>
#include <small_utils/utils_read_file.h>

#include <asio.hpp>

#include <random>
#include <algorithm>
#include <iterator>

#include <cstdio>
#include <cinttypes>

#if defined(NDEBUG)
#  undef NDEBUG
#endif
#include <cassert>

asio::awaitable<bool> PeerLoop(be::TorrentPeer& peer
    , be::PeerAddress address
    , std::size_t id
    , const be::TorrentClient& client)
{
    peer.socket_ = co_await peer.do_connect(address);
    if (!peer.socket_) { co_return false; }
    peer.info_ = co_await peer.do_handshake(client.info_hash_, client.peer_id_);
    if (!peer.info_) { co_return false; }

    peer.bitfield_ = co_await be::ReadMessage<be::Message_Bitfield>(*peer.socket_);
    if (!peer.bitfield_) { co_return false; }
    bool ok = co_await be::SendMessage(*peer.socket_, be::Message_Unchoke());
    if (!ok) { co_return false; }
    ok = co_await be::SendMessage(*peer.socket_, be::Message_Interested());
    if (!ok) { co_return false; }

    bool sent_sample = false;
    while (true)
    {
        be::AnyMessage m = co_await be::ReadAnyMessage(*peer.socket_);
        switch (m.index())
        {
        case 0/*std::monostate*/: co_return false;
        case 1/*Message_Choke*/: peer.unchocked_ = false; break;
        case 2/*Message_Unchoke*/: peer.unchocked_ = true; break;
        case 3/*Message_Interested*/: /*not implemented*/ break;
        case 4/*Message_NotInterested*/: /*not implemented*/ break;
        case 5/*Message_Have*/:
        {
            const auto& have = std::get<be::Message_Have>(m);
            if (!peer.bitfield_->set_piece(have.piece_index_))
            {
                co_return false;
            }
            break;
        }
        case 6/*Message_Bitfield*/: /*not possible*/ assert(false); break;
        case 7/*Message_Request*/: /*not implemented*/ break;
        case 8/*Message_Piece*/:
        {
            auto& piece = std::get<be::Message_Piece>(m);
            (void)piece;
            break;
        }
        case 9/*Message_Cancel*/: /*not implemented*/ break;
        case 10/*Message_KeepAlive*/: /*do nothing*/ break;
        case 11/*Message_Unknown*/: /*ignore*/ break;
        default: /*not possible, should be Message_Unknown*/ assert(false); break;
        }

        if (peer.unchocked_ && !sent_sample)
        {
            be::Message_Request request;
            const std::uint64_t k_max_block = 16384;
            request.length_ = std::uint32_t(std::min<std::uint64_t>(
                client.metainfo_.info_.piece_length_bytes_, k_max_block));
            request.piece_index_ = 0;
            request.offset_ = 0;
            ok = co_await be::SendMessage(*peer.socket_, request);
            if (!ok) { co_return false; }
        }
    }
    co_return true;
}

int main()
{
    const char* const torrent_file = R"(K:\debian-edu-10.6.0-amd64-netinst.iso.torrent)";
    std::random_device random;
    auto client = be::TorrentClient::make(torrent_file, random);
    assert(client);

    std::optional<be::TrackerResponse> data;

    {
        asio::io_context io_context(1);
        asio::co_spawn(io_context
            , client->request_torrent_peers(io_context)
            , [&data](std::exception_ptr, std::optional<be::TrackerResponse> response)
            {
                data = std::move(response);
            });

        io_context.run();
    }

    assert(data);
    auto info = std::get_if<be::TrackerResponse::OnSuccess>(&data->data_);
    assert(info);
    std::printf("Re-request seconds: %" PRIu64 "\n", info->rerequest_dt_secs_);
    std::printf("Peers count       : %zu\n", info->peers_.size());
    assert(!info->peers_.empty());

    asio::io_context io_context(1);
    std::vector<be::TorrentPeer> peers;
    for (const auto& _ : info->peers_)
    {
        (void)_;
        auto& peer = peers.emplace_back(be::TorrentPeer{});
        peer.io_context_ = &io_context;
    }

    std::size_t connected = 0;
    std::size_t failed = 0;
    for (std::size_t i = 0, count = info->peers_.size(); i < count; ++i)
    {
        asio::co_spawn(io_context
            , PeerLoop(peers[i], info->peers_[i], i, *client)
            , [i, &connected, &failed](std::exception_ptr, bool ok)
        {
            if (ok) { ++connected; }
            else { ++failed; }
        });
    }

    io_context.run();
    std::printf("Connected: %zu/%zu\n", connected, info->peers_.size());
    std::printf("Failed   : %zu/%zu\n", failed, info->peers_.size());

    auto how_many_has = [&](std::size_t piece)
    {
        std::size_t count = 0;
        for (const be::TorrentPeer& peer : peers)
        {
            if (!peer.bitfield_)
            {
                continue;
            }
            if (peer.bitfield_->has_piece(piece))
            {
                ++count;
            }
        }
        return count;
    };
    const std::size_t pieces_count = (client->metainfo_.info_.pieces_SHA1_.size() / sizeof(SHA1Bytes));
    const std::size_t bitfield_bytes = std::size_t((pieces_count + 0.5) / 8);
    for (std::size_t i = 0; i < pieces_count; ++i)
    {
        printf("Piece %zu has %zu peers\n", i, how_many_has(i));
    }
    return 0;
}
