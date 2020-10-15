#pragma once
#include <bencoding/be_torrent_file_parse.h>
#include <small_utils/utils_bytes.h>

namespace be
{
    struct TorrentClient
    {
        TorrentMetainfo metainfo_;
        SHA1Bytes info_hash_;
        PeerId peer_id_;

        static std::optional<TorrentClient> make(
            const char* torrent_file_path
            , std::random_device& random);
    };
} // namespace be
