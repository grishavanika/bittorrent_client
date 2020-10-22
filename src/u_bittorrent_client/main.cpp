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
#include <deque>
#include <queue>

#include <cstdio>
#include <cinttypes>

#if defined(NDEBUG)
#  undef NDEBUG
#endif
#include <cassert>

// As per https://www.bittorrent.org/beps/bep_0003.html
// All current implementations use 2^14 (16 kiB),
// and close connections which request an amount greater than that.
const std::uint32_t k_max_block = 16'384;
// How much Request(s) send before reading the piece.
const int k_max_backlog = 5;

struct Pieces
{
    struct PieceState
    {
        std::uint32_t piece_index_ = 0;
        std::uint32_t downloaded_ = 0;
        std::uint32_t requested_ = 0;

        // #QQQ: temporary.
        std::vector<std::uint8_t> data_;
    };

    // deque<> so references are not invalidated
    // on push_back().
    std::deque<PieceState> pieces_;
    std::deque<PieceState*> to_retry_;
    
    std::uint32_t pieces_count_ = 0;
    std::uint32_t piece_size_ = 0;
    std::uint64_t total_size_ = 0;

    std::uint32_t next_piece_index_ = 0;

    std::uint32_t get_piece_size(std::uint32_t piece_index) const
    {
        if (piece_index < (pieces_count_ - 1))
        {
            return piece_size_;
        }
        const std::uint64_t size = (piece_size_ * (pieces_count_ - 1));
        assert(total_size_ > size);
        return std::uint32_t(total_size_ - size);
    }

    PieceState* pop_piece_to_download()
    {
        if (next_piece_index_ < (pieces_count_ + 1))
        {
            std::uint32_t index = next_piece_index_++;
            pieces_.push_back({});
            PieceState* piece = &pieces_.back();
            piece->piece_index_ = index;
            piece->downloaded_ = 0;
            piece->requested_ = 0;
            return piece;
        }
        if (!to_retry_.empty())
        {
            PieceState* piece = to_retry_.front();
            to_retry_.pop_front();
            return piece;
        }
        return nullptr;
    }

    void push_piece_to_retry(PieceState& piece)
    {
        // Re-download all piece.
        piece.downloaded_ = 0;
        piece.requested_ = 0;
        piece.data_.clear();
        to_retry_.push_back(&piece);
    }
};

///////////////////////////////////////////////////////////////////////////////
// #QQQ: temporary for debug.

static std::uint64_t _x_downloaded_bytes_ = 0;
static std::uint32_t _x_downloaded_pieces_ = 0;

static std::uint64_t _x_total_bytes_ = 0;
static std::uint32_t _x_total_pieces_ = 0;
static std::uint32_t _x_total_peers_ = 0;
static std::uint32_t _x_active_peers_ = 0;

void NOTIFY_DOWNLOAD(const Pieces& pieces, const Pieces::PieceState& piece)
{
    _x_downloaded_bytes_ += piece.downloaded_;
    _x_downloaded_pieces_ += 1;

    const float percents = ((_x_downloaded_bytes_ * 100.f) / _x_total_bytes_);

    printf("[%" PRIu32 "][%.2f %%]. "
        "Pieces %" PRIu32 "/%" PRIu32 ". "
        "Bytes %" PRIu64 "/%" PRIu64 ". "
        "Peers %" PRIu32 "/%" PRIu32 ". "
        "Retry queue size: %zu."
        "\n"
        , piece.piece_index_, percents
        , _x_downloaded_pieces_, _x_total_pieces_
        , _x_downloaded_bytes_, _x_total_bytes_
        , _x_active_peers_, _x_total_peers_
        , pieces.to_retry_.size());
}

asio::awaitable<bool> TryDownloadPiecesFromPeer(
    be::TorrentPeer& peer, Pieces& pieces)
{
    // Mostly from https://blog.jse.li/posts/torrent/.
    // E.g.: https://github.com/veggiedefender/torrent-client/blob/master/p2p/p2p.go.
    while (true)
    {
        Pieces::PieceState* piece = pieces.pop_piece_to_download();
        if (!piece)
        {
            // #QQQ: not quite correct. Some pieces may be in progress,
            // but then fail to complete. We need some peers to be
            // active to re-schedule the piece.
            co_return false;
        }
        // Put back to the queue on early out or
        // coroutine exit.
        auto retry = folly::makeGuard([piece, &pieces] { pieces.push_piece_to_retry(*piece); });

        if (!peer.bitfield_.has_piece(piece->piece_index_))
        {
            // Try another one. Peer has no such a piece.
            continue;
        }

        const std::uint32_t piece_size = pieces.get_piece_size(piece->piece_index_);
        int backlog = 0;
        while (piece->downloaded_ < piece_size)
        {
            if (peer.unchocked_)
            {
                if ((backlog < k_max_backlog)
                    && (piece->requested_ < piece_size))
                {
                    const std::uint32_t needed = (piece_size - piece->requested_);
                    const std::uint32_t block_size = std::min(k_max_block, needed);

                    be::Message_Request request;
                    request.piece_index_ = piece->piece_index_;
                    request.offset_ = piece->requested_;
                    request.length_ = block_size;
                    auto sent = co_await SendMessage(peer.socket_, request);
                    if (!sent) { co_return false; }
                    
                    ++backlog;
                    piece->requested_ += block_size;
                }
            }

            auto m_any = co_await be::ReadAnyMessage(peer.socket_);
            if (!m_any) { co_return false; }
            be::AnyMessage& m = m_any.value();
            switch (m.index())
            {
            case 1/*Message_Choke*/:
                peer.unchocked_ = false;
                break;
            case 2/*Message_Unchoke*/:
                peer.unchocked_ = true;
                break;
            case 3/*Message_Interested*/:
                // not implemented
                assert(false && "Message_Interested: not implemented");
                break;
            case 4/*Message_NotInterested*/:
                // not implemented
                assert(false && "Message_NotInterested: not implemented");
                break;
            case 5/*Message_Have*/:
            {
                const auto& have = std::get<be::Message_Have>(m);
                if (!peer.bitfield_.set_piece(have.piece_index_))
                {
                    assert(false && "Failed to set HAVE piece");
                    // Non-critical, continue.
                }
                break;
            }
            case 6/*Message_Bitfield*/:
                // not possible
                assert(false && "Bitfield should be received only once, at the beginning");
                break;
            case 7/*Message_Request*/:
                // not implemented
                assert(false && "Message_Request: not implemented");
                break;
            case 8/*Message_Piece*/:
            {
                auto& msg = std::get<be::Message_Piece>(m);

                const std::uint32_t count = msg.size();
                assert((msg.piece_index_ == piece->piece_index_)
                    && "Mixed order of pieces");
                assert((count > 0) && "Piece with zero size");
                assert((msg.piece_begin_ == piece->downloaded_)
                    && "Out of order piece with wrong offset");
                piece->downloaded_ += count;
                assert((piece->downloaded_ <= piece_size)
                    && "Downloaded more then piece has in size");

                const std::size_t prev = piece->data_.size();
                piece->data_.resize(prev + count);
                std::memcpy(&piece->data_[prev], msg.data(), count);

                --backlog;
                break;
            }
            case 9/*Message_Cancel*/:
                // not implemented
                assert(false && "Message_Cancel: not implemented");
                break;
            case 10/*Message_KeepAlive*/:
                // do nothing
                break;
            case 11/*Message_Unknown*/:
                // ignore
                break;
            default: /*not possible, should be Message_Unknown*/
                assert(false);
                break;
            }
        }
        
        assert(piece->downloaded_ == piece_size);

        retry.dismiss();
        NOTIFY_DOWNLOAD(pieces, *piece);

        // # UUU: validate and retry on hash mismatch.
        be::Message_Have have;
        have.piece_index_ = piece->piece_index_;
        auto sent = co_await SendMessage(peer.socket_, have);
        if (!sent) { co_return false; }
    }
}

asio::awaitable<bool> DownloadFromPeer(
    asio::io_context& io_context
    , const be::TorrentClient& client
    , const be::PeerAddress& address
    , Pieces& pieces
    , be::TorrentPeer& peer)
{
    auto started = co_await peer.start(address, client.info_hash_, client.peer_id_);
    if (!started) { co_return false; }
    auto bitfield = co_await be::ReadMessage<be::Message_Bitfield>(peer.socket_);
    if (!bitfield) { co_return false; }
    peer.bitfield_ = std::move(bitfield.value());

    auto sent = co_await be::SendMessage(peer.socket_, be::Message_Unchoke());
    if (!sent) { co_return false; }
    sent = co_await be::SendMessage(peer.socket_, be::Message_Interested());
    if (!sent) { co_return false; }

    co_return co_await TryDownloadPiecesFromPeer(peer, pieces);
}

int main()
{
    const char* const torrent_file = R"(K:\debian-mac-10.6.0-amd64-netinst.iso.torrent)";
    std::random_device random;
    auto client = be::TorrentClient::make(torrent_file, random).value();

    { // Validate size & pieces count & piece size.
        const std::uint32_t pieces_count = client.get_pieces_count();
        const std::uint32_t piece_size = client.get_piece_size_bytes();
        const std::uint64_t total_size = client.get_total_size_bytes();
        assert(pieces_count > 0);
        assert(piece_size > 0);
        const std::uint64_t size_except_last_piece = (std::uint64_t(piece_size) * std::uint64_t((pieces_count - 1)));
        assert(size_except_last_piece < total_size);
        const std::uint64_t last = (total_size - size_except_last_piece);
        assert(last <= std::uint64_t(piece_size));
    }

    be::TrackerResponse tracker;

    { // Get the info from the tracker first.
        asio::io_context io_context(1);
        asio::co_spawn(io_context
            , [&]() -> asio::awaitable<void>
            {
                auto data = co_await client.request_torrent_peers(io_context);
                assert(data);
                tracker = std::move(data.value());
                co_return;
            }
            , asio::detached);

        io_context.run();
    }

    auto tracker_info = std::get_if<be::TrackerResponse::OnSuccess>(&tracker.data_);
    assert(tracker_info);
    std::printf("Re-request seconds: %" PRIu64 "\n", tracker_info->rerequest_dt_secs_);
    std::printf("Peers count       : %zu\n", tracker_info->peers_.size());
    assert(!tracker_info->peers_.empty());

    asio::io_context io_context(1);
    std::vector<be::TorrentPeer> peers;
    for (const auto& _ : tracker_info->peers_)
    {
        (void)_;
        peers.emplace_back(io_context);
    }

    Pieces pieces;
    pieces.pieces_count_ = client.get_pieces_count();
    pieces.piece_size_ = client.get_piece_size_bytes();
    pieces.total_size_ = client.get_total_size_bytes();

    _x_total_bytes_ = pieces.total_size_;
    _x_total_pieces_ = pieces.pieces_count_;
    _x_active_peers_ = std::uint32_t(tracker_info->peers_.size());
    _x_total_peers_ = _x_active_peers_;

    std::default_random_engine rng{random()};
    std::shuffle(std::begin(tracker_info->peers_), std::end(tracker_info->peers_), rng);

    for (std::size_t i = 0, count = tracker_info->peers_.size(); i < count; ++i)
    {
        asio::co_spawn(io_context
            , DownloadFromPeer(io_context, client, tracker_info->peers_[i], pieces, peers[i])
            , [](std::exception_ptr, bool)
        {
            --_x_active_peers_;
        });
    }

    io_context.run();
    return 0;
}
