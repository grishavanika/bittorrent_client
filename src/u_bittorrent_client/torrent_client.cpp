#include "torrent_client.h"
#include <bencoding/be_torrent_file_parse.h>
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_bytes.h>

namespace be
{
    static std::string_view AsStringView(const FileBuffer& buffer, ElementPosition p)
    {
        assert(p.start_ < p.end_);
        assert(p.end_ <= buffer.size_);
        const auto start = (static_cast<const char*>(buffer.data_) + p.start_);
        return std::string_view(start, (p.end_ - p.start_));
    }

    static std::string_view AsStringView(const FileBuffer& buffer)
    {
        return std::string_view(static_cast<const char*>(buffer.data_), buffer.size_);
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
            client->info_hash_ = GetSHA1(AsStringView(buffer, torrent->info_position_));
        }

        client->peer_id_ = GetRandomPeerId(random);
        return client;
    }
} // namespace be
