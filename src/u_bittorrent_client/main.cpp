#include "torrent_client.h"

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
    return 0;
}
