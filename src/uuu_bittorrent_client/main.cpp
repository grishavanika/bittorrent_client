#include <bencoding/be_torrent_file_parse.h>
#include "torrent_client.h"
#include <small_utils/utils_read_file.h>
#include <small_utils/utils_url.h>

#include <random>

#include <cstdio>

#if defined(NDEBUG)
#  undef NDEBUG
#endif
#include <cassert>

int main()
{
    const char* const torrent_file = R"(K:\debian-edu-10.6.0-amd64-netinst.iso.torrent)";
    std::random_device random;
    auto client = be::TorrentClient::make(torrent_file, random);
    assert(client);

    const std::size_t pieces = (client->metainfo_.info_.pieces_SHA1_.size() / 20);
    // Cool work with URLs in C++.
    const std::string url =
        client->metainfo_.tracker_url_utf8_ + "?"
        + "info_hash="  + UrlEscape(client->info_hash_.data_, sizeof(client->info_hash_.data_)) + "&"
        + "peer_id="    + UrlEscape(client->peer_id_.data_, sizeof(client->peer_id_.data_)) + "&"
        + "port="       + "6882" + "&"
        + "uploaded="   + "0" + "&"
        + "downloaded=" + "0" + "&"
        + "compact="    + "1" + "&"
        + "left="       + std::to_string(pieces);
    printf("%s\n", url.c_str());
}
