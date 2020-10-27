#include "torrent_client.h"
#include "torrent_messages.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_tracker_response_parse.h>
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_experimental.h>

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
        PieceState(std::uint32_t index) : piece_index_(index) {}
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
            (void)pieces_.emplace_back(next_piece_index_++);
            // We don't check `have_pieces` for a reason:
            // let's the caller decide if it needs to retry or stop connection.
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

asio::awaitable<outcome::result<void>> TryDownloadPiecesFromPeer(
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
            co_return outcome::failure(ClientErrorc::TODO);
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
                    OUTCOME_CO_TRY(co_await SendMessage(peer.socket_, request));
                    
                    ++backlog;
                    piece->requested_ += block_size;
                }
            }

            OUTCOME_CO_TRY(msg, co_await be::ReadAnyMessage(peer.socket_));

            std::visit(overload{
                  [ ](be::Message_KeepAlive&) { }
                , [&](be::Message_Choke&)     { peer.unchocked_ = false; }
                , [&](be::Message_Unchoke&)   { peer.unchocked_ = true; }
                , [&](be::Message_Have& have) { (void)peer.bitfield_.set_piece(have.piece_index_); }
                , [&](be::Message_Piece& msg_piece)
                {
                    --backlog;

                    const std::uint32_t data_size = msg_piece.size();
                    assert((msg_piece.piece_index_ == piece->piece_index_)
                        && "Mixed order of pieces");
                    assert((data_size > 0) && "Piece with zero size");
                    assert((msg_piece.piece_begin_ == piece->downloaded_)
                        && "Out of order piece with wrong offset");
                    piece->downloaded_ += data_size;
                    assert((piece->downloaded_ <= piece_size)
                        && "Downloaded more then piece has in size");
                    const std::size_t prev = piece->data_.size();
                    piece->data_.resize(prev + data_size);
                    std::memcpy(&piece->data_[prev], msg_piece.data(), data_size);
                }
                , [](auto&) { assert(false && "Unhandled message from peer"); }
                }, msg);
        }
        
        assert(piece->downloaded_ == piece_size);

        const std::uint32_t piece_index = piece->piece_index_;
        retry.dismiss();
        pieces.on_piece_downloaded(piece);

        // # UUU: validate and retry on hash mismatch.
        be::Message_Have have;
        have.piece_index_ = piece_index;
        OUTCOME_CO_TRY(co_await SendMessage(peer.socket_, have));
    }
}

// Can't use outcome::result<void> because ASIO needs
// return type to be default constructible if used
// in asio::co_spawn().
asio::awaitable<std::error_code> DownloadFromPeer(
    asio::io_context& io_context
    , const be::TorrentClient& client
    , be::PeerAddress address
    , PiecesToDownload& pieces
    , be::TorrentPeer& peer)
{
    OUTCOME_CO_TRY_ERR(co_await peer.start(address, client.info_hash_, client.peer_id_));
    OUTCOME_CO_TRY_ERRV(bitfield, co_await be::ReadMessage<be::Message_Bitfield>(peer.socket_));
    peer.bitfield_ = std::move(bitfield);
    OUTCOME_CO_TRY_ERR(co_await be::SendMessage(peer.socket_, be::Message_Unchoke()));
    OUTCOME_CO_TRY_ERR(co_await be::SendMessage(peer.socket_, be::Message_Interested()));
    OUTCOME_CO_TRY_ERR(co_await TryDownloadPiecesFromPeer(peer, pieces));
    co_return ClientErrorc::Ok;
}

void DoOneTrackerRound(be::TorrentClient& client, PiecesToDownload& pieces)
{
    be::TorrentClient::RequestInfo request;
    request.server_port = 6882;
    request.pieces_left = (pieces.pieces_count_ - pieces.downloaded_pieces_count_);
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
    peers.reserve(tracker_info->peers_.size());
    for (auto address : tracker_info->peers_)
    {
        peers.emplace_back(io_context);
        asio::co_spawn(io_context
            , DownloadFromPeer(io_context, client, address, pieces, peers.back())
            , asio::detached);
    }

    io_context.run();
}

struct FileOffset
{
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::size_t file_index = 0;
};

struct FilePiece
{
    std::size_t file_index_;
    const std::string* file_name_ = nullptr;
    std::uint64_t file_offset_ = 0;
    std::uint64_t bytes_count_ = 0;
    std::uint64_t piece_offset_ = 0;
    std::uint64_t file_size_ = 0;
};

struct FilesList
{
    const be::TorrentClient* torrent_;
    std::vector<FileOffset> files_offset_;

    static FilesList make(const be::TorrentClient& torrent)
    {
        using LengthOrFiles = be::TorrentMetainfo::LengthOrFiles;
        using File = be::TorrentMetainfo::File;
        const LengthOrFiles& data = torrent.metainfo_.info_.length_or_files_;

        FilesList list;
        list.torrent_ = &torrent;
        if (const std::uint64_t* single_file = std::get_if<std::uint64_t>(&data))
        {
            list.files_offset_.push_back({});
            FileOffset& offset = list.files_offset_.back();
            offset.file_index = 0;
            offset.start = 0;
            offset.end = *single_file;
        }
        else if (const auto* multi_files
            = std::get_if<std::vector<File>>(&data))
        {
            list.files_offset_.reserve(multi_files->size());
            FileOffset fo;
            fo.start = 0;
            for (std::size_t index = 0, count = multi_files->size(); index < count; ++index)
            {
                const File& file = (*multi_files)[index];
                fo.end = fo.start + file.length_bytes_;
                fo.file_index = index;
                list.files_offset_.push_back(fo);
                fo.start = fo.end;
            }
        }
        else
        {
            assert(false && "Invalid torrent metainfo.");
        }
        assert(list.files_offset_.size() > 0);
        assert(list.files_offset_.back().end == torrent.get_total_size_bytes());
        return list;
    }

    // F(const FilePiece& file_piece)
    template<typename F>
    void iterate_files(std::uint64_t start_bytes, std::uint64_t end_bytes, F f) const
    {
        assert(end_bytes > start_bytes);
        assert(files_offset_.size() > 0);

        auto end = files_offset_.end();
        auto it_start = std::lower_bound(files_offset_.begin(), end, start_bytes
            , [](const FileOffset& lhs, std::uint64_t rhs) { return (lhs.start < rhs); });
        if (it_start == end)
        {
            // File with start >= start_bytes. Must be the last one.
            it_start = (end - 1);
        }
        if (it_start->start > start_bytes)
        {
            // Found the file with start > start_bytes.
            // File that has start < start_bytes is previous one.
            --it_start;
        }
        // start_bytes should be in file's [start; end).
        assert((start_bytes >= it_start->start)
            && (start_bytes < it_start->end));

        // Find the file where the end_bytes is. It starts from
        // already found it_start file for sure.
        auto it_end = std::lower_bound(it_start, end, end_bytes
            , [](const FileOffset& lhs, std::uint64_t rhs)
                { return (lhs.end < rhs); });
        // There is always file with end >= end_bytes.
        // Otherwise caller tries to write past the end of the file.
        assert(it_end != end);
        // Either the same or next file(s).
        assert(it_end >= it_start);
        // end_bytes should be in file's (start; end].
        assert((end_bytes > it_end->start)
            && (end_bytes <= it_end->end));

        using File = be::TorrentMetainfo::File;
        const auto* files = std::get_if<std::vector<File>>(
            &torrent_->metainfo_.info_.length_or_files_);

        std::uint64_t data_offset = 0;
        // Iterate thru all files that overlap with [start_bytes; end_bytes].
        for (auto it = it_start; it != (it_end + 1); ++it)
        {
            const FileOffset& fo = *it;
            const std::string& file_name = files
                ? (*files)[fo.file_index].path_utf8_
                : torrent_->metainfo_.info_.suggested_name_utf8_;
            const std::uint64_t file_size = files
                ? (*files)[fo.file_index].length_bytes_
                : files_offset_[0].end; // we know we have single file.

            const std::uint64_t start_offset = std::max(start_bytes, fo.start);
            const std::uint64_t end_offset = std::min(fo.end, end_bytes);

            FilePiece piece;
            piece.file_index_ = fo.file_index;
            piece.file_name_ = &file_name;
            piece.file_offset_ = (start_offset - fo.start);
            piece.bytes_count_ = (end_offset - start_offset);
            piece.piece_offset_ = data_offset;
            piece.file_size_ = file_size;

            f(piece);
            // Consumed part of the input range.
            data_offset += piece.bytes_count_;
        }
        // We should consume all the range passed.
        assert(data_offset == (end_bytes - start_bytes));
    }

    // F(const FilePiece& file_piece)
    template<typename F>
    void iterate_files(std::uint32_t piece_index, F f) const
    {
        assert(files_offset_.size() > 0);
        const std::uint64_t total_size = files_offset_.back().end;
        const std::uint64_t start = piece_index * torrent_->get_piece_size_bytes();
        std::uint64_t end = (piece_index + 1) * torrent_->get_piece_size_bytes();
        end = std::min(end, total_size);
        iterate_files(start, end, std::move(f));
    }
};

#include <Windows.h>

struct PhysicalFile
{
    HANDLE file_ = INVALID_HANDLE_VALUE;
    std::uint64_t written_ = 0;

    PhysicalFile() = default;
    PhysicalFile(const PhysicalFile&) = delete;
    PhysicalFile& operator=(const PhysicalFile&) = delete;
    PhysicalFile(PhysicalFile&& rhs) noexcept
        : file_(std::exchange(rhs.file_, INVALID_HANDLE_VALUE))
        , written_(0)
    {
    }
    PhysicalFile& operator=(PhysicalFile&& rhs)
    {
        if (this != &rhs)
        {
            close();
            file_ = std::exchange(rhs.file_, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    ~PhysicalFile()
    {
        close();
    }
    void close()
    {
        if (file_ != INVALID_HANDLE_VALUE)
        {
            (void)::CloseHandle(file_);
            file_ = INVALID_HANDLE_VALUE;
        }
    }

    void try_create(const std::string& name, std::uint64_t final_size)
    {
        if (file_ != INVALID_HANDLE_VALUE)
        {
            return;
        }
        file_ = ::CreateFileA(name.c_str()
            , GENERIC_WRITE
            , 0 // do not share with anyone
            , nullptr
            , CREATE_NEW // fail if already exists
            , FILE_ATTRIBUTE_NORMAL
            , nullptr);
        assert(file_);
        LARGE_INTEGER size{};
        size.QuadPart = static_cast<LONGLONG>(final_size);
        BOOL ok = ::SetFilePointerEx(file_, size, nullptr, FILE_BEGIN);
        assert(ok);
        ok = ::SetEndOfFile(file_);
        assert(ok);
    }

    void write(const void* data, std::uint64_t offset, std::uint32_t size)
    {
        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(offset);
        BOOL ok = ::SetFilePointerEx(file_, li, nullptr, FILE_BEGIN);
        assert(ok);

        DWORD written = 0;
        ok = ::WriteFile(file_, data, size, &written, nullptr);
        assert(ok);
        assert(written == size);
        written_ += size;
    }

    void try_finalize(std::uint64_t final_size)
    {
        assert(written_ <= final_size);
        if (written_ == final_size)
        {
            close();
        }
    }
};

struct FilesOnDisk
{
    std::vector<PhysicalFile> files_;
    const FilesList* files_list_;

    FilesOnDisk(const FilesList& files_list)
        : files_()
        , files_list_(&files_list)
    {
        files_.resize(files_list_->files_offset_.size());
    }

    void write_piece(PiecesToDownload::PieceState& piece)
    {
        assert(piece.data_.size() > 0);
        files_list_->iterate_files(piece.piece_index_
            , [this, &piece](const FilePiece& file_piece)
        {
            on_write_file_piece(file_piece, &piece.data_[0], piece.data_.size());
        });
        piece.data_ = {};
    }

    void on_write_file_piece(const FilePiece& piece
        , const std::uint8_t* all_data, std::size_t data_size)
    {
        assert(piece.file_index_ < files_.size());
        assert(piece.piece_offset_ < data_size);
        assert(piece.bytes_count_ > 0);
        PhysicalFile& f = files_[piece.file_index_];
        f.try_create(*piece.file_name_, piece.file_size_);
        f.write(all_data + piece.piece_offset_
            , piece.file_offset_
            , std::uint32_t(piece.bytes_count_));
        f.try_finalize(piece.file_size_);
    }

    std::uint64_t total_written() const
    {
        std::uint64_t total = 0;
        for (const PhysicalFile& f : files_)
        {
            total += f.written_;
        }
        return total;
    }
};

int main(int argc, char* argv[])
{
    assert(argc == 2);
    const char* torrent_file = argv[1];

    std::random_device random;
    auto client = be::TorrentClient::make(torrent_file, random);
    assert(client);
    auto& client_ref = client.value();
    auto files_list = FilesList::make(client_ref);
    auto files_on_disk = FilesOnDisk(files_list);

    PiecesToDownload pieces;
    pieces.pieces_count_ = client_ref.get_pieces_count();
    pieces.piece_size_ = client_ref.get_piece_size_bytes();
    pieces.total_size_ = client_ref.get_total_size_bytes();
    pieces.downloaded_pieces_count_ = 0;
    pieces.next_piece_index_ = 0;
    pieces.on_new_piece = std::bind(&FilesOnDisk::write_piece, &files_on_disk, std::placeholders::_1);

    while (pieces.downloaded_pieces_count_ < pieces.pieces_count_)
    {
        DoOneTrackerRound(client_ref, pieces);
    }

    assert(pieces.downloaded_pieces_count_ == pieces.pieces_count_);
    assert(pieces.pieces_.empty());
    assert(pieces.to_retry_.empty());
    assert(files_on_disk.total_written() == pieces.total_size_);
    return 0;
}
