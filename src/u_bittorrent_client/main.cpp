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

// From:
// coroutines_ts/echo_server.cpp:
// https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp17/coroutines_ts/echo_server.cpp
// http/async_client.cpp:
// https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp03/http/client/async_client.cpp
asio::awaitable<std::optional<be::TrackerResponse>>
    RequestTorrentPeers(be::TorrentClient& client, asio::io_context& io_context)
{
    asio::ip::tcp::resolver resolver_(io_context);
    asio::ip::tcp::socket socket_(io_context);
    asio::streambuf request_;
    asio::streambuf response_;

    // #UUU: temporary use asserts().
    auto http = client.get_tracker_request_info();
    assert(http);
    const std::string port = std::to_string(http->port_);
    const std::string server = http->host_ + ':' + port;

    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    std::ostream request_stream(&request_);
    request_stream << "GET " << http->get_uri_ << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    std::error_code ec;
    auto coro = [&ec] { return asio::redirect_error(asio::use_awaitable, ec); };

    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    asio::ip::tcp::resolver::results_type endpoints =
        co_await resolver_.async_resolve(http->host_, port, coro());
    assert(!ec);
    // Attempt a connection to each endpoint in the list until we
    // successfully establish a connection.
    co_await asio::async_connect(socket_, std::move(endpoints), coro());
    assert(!ec);
    // The connection was successful. Send the request.
    co_await asio::async_write(socket_, request_, coro());
    assert(!ec);
    // Read the response status line. The response_ streambuf will
    // automatically grow to accommodate the entire line. The growth may be
    // limited by passing a maximum size to the streambuf constructor.
    co_await asio::async_read_until(socket_, response_, "\r\n", coro());
    assert(!ec);

    // Check that response is OK.
    std::istream response_stream(&response_);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code = 0;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
        co_return std::nullopt;
    }
    if (status_code != 200)
    {
        co_return std::nullopt;
    }

    // Read the response headers, which are terminated by a blank line.
    co_await asio::async_read_until(socket_, response_, "\r\n\r\n", coro());
    assert(!ec);

    // Process the response headers.
    std::istream headers_stream(&response_);
    std::string header;
    while (std::getline(headers_stream, header)
        && (header != "\r"))
    {
        // Ignore any header.
    }

    std::string content;
    auto add_content = [&content](asio::streambuf& incoming)
    {
        content.reserve(content.size() + incoming.size());
        std::istream ss(&incoming);
        std::copy(std::istreambuf_iterator<char>(ss), {}
            , std::back_inserter(content));
    };
    // Write whatever content we already have to output.
    add_content(response_);

    while (!ec)
    {
        co_await asio::async_read(socket_, response_,
            asio::transfer_at_least(1), coro());
        add_content(response_);
    }
    assert(ec == asio::error::eof);

    co_return be::ParseTrackerCompactResponseContent(content);
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
            , RequestTorrentPeers(*client, io_context)
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
