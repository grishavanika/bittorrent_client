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
#include <list>
#include <functional>

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

// Stupid and simple algorithm to distribute
// N pieces needed to download, sequentially.
struct PiecesToDownload
{
    struct PieceState
    {
        std::uint32_t piece_index_ = 0;
        std::uint32_t downloaded_ = 0;
        std::uint32_t requested_ = 0;

        std::vector<std::uint8_t> data_;
    };

    // list<> so references and iterators are not invalidated.
    // Looks like perfect match for this task.
    std::list<PieceState> pieces_;
    using Handle = std::list<PieceState>::iterator;
    std::vector<Handle> to_retry_;
    
    std::uint32_t pieces_count_ = 0;
    std::uint32_t piece_size_ = 0;
    std::uint64_t total_size_ = 0;

    std::uint32_t next_piece_index_ = 0;
    std::uint32_t downloaded_pieces_count_ = 0;
    std::function<void (PieceState&)> on_new_piece;

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

    Handle pop_piece_to_download(const be::Message_Bitfield& have_pieces)
    {
        if (next_piece_index_ < pieces_count_)
        {
            const std::uint32_t piece_index = next_piece_index_++;
            pieces_.push_back({});
            PieceState* piece = &pieces_.back();
            piece->piece_index_ = piece_index;
            piece->downloaded_ = 0;
            piece->requested_ = 0;

            // We don't check `have_pieces` for a reason:
            // let's caller decide if it needs to retry or stop
            // connection.
            auto handle = pieces_.end();
            --handle; // to the last element
            return handle;
        }
        if (to_retry_.empty())
        {
            return pieces_.end();
        }

        for (auto it = to_retry_.begin(); it != to_retry_.end(); ++it)
        {
            // Check a caller has a piece.
            // In case caller does not have **all** retry pieces
            // we'll return fail to notify the caller there is
            // no need to try again.
            if (!have_pieces.has_piece((**it).piece_index_))
            {
                continue;
            }
            Handle piece = *it;
            to_retry_.erase(it);
            return piece;
        }
        return pieces_.end();
    }

    void push_piece_to_retry(Handle piece)
    {
        // Re-download all piece.
        piece->downloaded_ = 0;
        piece->requested_ = 0;
        piece->data_.clear();
        to_retry_.push_back(piece);
    }

    void on_piece_downloaded(Handle piece)
    {
        ++downloaded_pieces_count_;
        assert(on_new_piece);
        assert(piece != pieces_.end());
        on_new_piece(*piece);
        pieces_.erase(piece);
    }
};

asio::awaitable<bool> TryDownloadPiecesFromPeer(
    be::TorrentPeer& peer, PiecesToDownload& pieces)
{
    // Mostly from https://blog.jse.li/posts/torrent/.
    // E.g.: https://github.com/veggiedefender/torrent-client/blob/master/p2p/p2p.go.
    while (true)
    {
        PiecesToDownload::Handle piece = pieces.pop_piece_to_download(peer.bitfield_);
        if (piece == pieces.pieces_.end())
        {
            // We stop and terminate connection.
            co_return false;
        }
        // Put back to the queue on early out or
        // coroutine exit.
        auto retry = folly::makeGuard([piece, &pieces]
        {
            pieces.push_piece_to_retry(piece);
        });

        if (!peer.bitfield_.has_piece(piece->piece_index_))
        {
            // Try another one. Peer has no such a piece.
            // Connection is still used: peer may have other pieces.
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

        const std::uint32_t piece_index = piece->piece_index_;
        retry.dismiss();
        pieces.on_piece_downloaded(piece);

        // # UUU: validate and retry on hash mismatch.
        be::Message_Have have;
        have.piece_index_ = piece_index;
        auto sent = co_await SendMessage(peer.socket_, have);
        if (!sent) { co_return false; }
    }
}

asio::awaitable<bool> DownloadFromPeer(
    asio::io_context& io_context
    , const be::TorrentClient& client
    , const be::PeerAddress& address
    , PiecesToDownload& pieces
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

void DoOneTrackerRound(be::TorrentClient& client, PiecesToDownload& pieces)
{
    be::TorrentClient::RequestInfo request;
    request.server_port = 6882;
    request.pieces_count = pieces.pieces_count_;
    request.downloaded_pieces = pieces.downloaded_pieces_count_;
    request.uploaded_pieces = 0;

    be::TrackerResponse tracker;
    { // Get the info from the tracker first.
        asio::io_context io_context(1);
        asio::co_spawn(io_context
            , [&]() -> asio::awaitable<void>
            {
                auto data = co_await client.request_torrent_peers(io_context, request);
                assert(data);
                tracker = std::move(data.value());
                co_return;
            }
            , asio::detached);

        io_context.run();
    }

    auto tracker_info = std::get_if<be::TrackerResponse::OnSuccess>(&tracker.data_);
    assert(tracker_info);
    assert(!tracker_info->peers_.empty());

    asio::io_context io_context(1);
    std::vector<be::TorrentPeer> peers;
    for (auto address : tracker_info->peers_)
    {
        peers.emplace_back(io_context);
        asio::co_spawn(io_context
            , DownloadFromPeer(io_context, client, address, pieces, peers.back())
            , asio::detached);
    }

    io_context.run();
}

void WriteAllPiecesToFile(const std::vector<PiecesToDownload::PieceState>& pieces, const char* output_file)
{
#if (_MSC_VER)
    FILE* f = nullptr;
    const errno_t e = fopen_s(&f, output_file, "wb");
    (void)e;
#else
    FILE* const f = fopen(output_file, "wb");
#endif
    if (!f)
    {
        return;
    }
    SCOPE_EXIT{(void)fclose(f);};

    for (const auto& state : pieces)
    {
        assert(state.data_.size() > 0);
        const size_t written = fwrite(&state.data_[0], 1, state.data_.size(), f);
        assert(written == state.data_.size());
    }
}

int main()
{
    const char* const torrent_file = R"(K:\debian-mac-10.6.0-amd64-netinst.iso.torrent)";
    const char* const output_file = R"(K:\torrent.iso)";

    std::random_device random;
    auto client = be::TorrentClient::make(torrent_file, random);
    assert(client);
    auto& client_ref = client.value();

    PiecesToDownload pieces;
    using PieceState = PiecesToDownload::PieceState;
    pieces.pieces_count_ = client_ref.get_pieces_count();
    pieces.piece_size_ = client_ref.get_piece_size_bytes();
    pieces.total_size_ = client_ref.get_total_size_bytes();
    pieces.downloaded_pieces_count_ = 0;
    pieces.next_piece_index_ = 0;

    std::vector<PieceState> downloaded_data;
    pieces.on_new_piece = [&](PieceState& piece)
    {
        downloaded_data.push_back(std::move(piece));
    };

    while (pieces.downloaded_pieces_count_ < pieces.pieces_count_)
    {
        DoOneTrackerRound(client_ref, pieces);
    }
    assert(pieces.downloaded_pieces_count_ == pieces.pieces_count_);
    assert(pieces.pieces_.empty());
    assert(pieces.to_retry_.empty());
    assert(downloaded_data.size() == client_ref.get_pieces_count());

    std::sort(downloaded_data.begin(), downloaded_data.end()
        , [](const PieceState& lhs, const PieceState& rhs)
        {
            return (lhs.piece_index_ < rhs.piece_index_);
        });
    WriteAllPiecesToFile(downloaded_data, output_file);
    return 0;
}
