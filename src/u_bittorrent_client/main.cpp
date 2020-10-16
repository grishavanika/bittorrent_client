#include "torrent_client.h"

#include <bencoding/be_torrent_file_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_tracker_response_parse.h>
#include <small_utils/utils_read_file.h>

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
    auto http = client->get_tracker_request_info();
    assert(http);
    
    printf("%s:%u%s\n", http->host_.c_str(), unsigned(http->port_), http->get_uri_.c_str());

    const char* const response_file = R"(K:\sample.bin)";
    const FileBuffer buffer = ReadAllFileAsBinary(response_file);
    assert(buffer.data_);
    std::optional<be::TrackerResponse> response = be::ParseTrackerCompactResponseContent(
        std::string_view(static_cast<const char*>(buffer.data_), buffer.size_));
    assert(response.has_value());
}
