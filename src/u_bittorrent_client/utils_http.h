#pragma once
#include <asio.hpp>

#include <optional>
#include <string>
#include <iostream>
#include <system_error>
#include <algorithm>
#include <iterator>

#include <cstdint>

#include "leaf_co_asio_wrapper.h"

namespace http
{
    struct Error_BadResponseStream {};
    struct Error_Version { std::string version; };
    struct Error_StatusCode { unsigned int status_code; };
} // namespace http

// From:
// coroutines_ts/echo_server.cpp:
// https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp17/coroutines_ts/echo_server.cpp
// http/async_client.cpp:
// https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp03/http/client/async_client.cpp
inline asio::awaitable<boost::leaf::result<std::string>>
    HTTP_GET(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n = 80)
{
    asio::ip::tcp::resolver resolver(io_context);
    asio::streambuf request;
    asio::ip::tcp::socket socket(io_context);
    asio::streambuf response;

    const std::string port = std::to_string(port_n);
    const std::string server = (host + ':' + port);

    std::ostream request_stream(&request);
    request_stream << "GET " << get_uri << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    auto co_leaf = as_leaf_result(asio::use_awaitable);

    LEAF_CO_AUTO(endpoints, co_await resolver.async_resolve(host, port, co_leaf));
    LEAF_CO_CHECK(co_await asio::async_connect(socket, std::move(endpoints), co_leaf));
    LEAF_CO_CHECK(co_await asio::async_write(socket, request, co_leaf));
    LEAF_CO_CHECK(co_await asio::async_read_until(socket, response, "\r\n", co_leaf));

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code = 0;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream)
    {
        co_return boost::leaf::new_error(http::Error_BadResponseStream{});
    }
    if (http_version.substr(0, 5) != "HTTP/")
    {
        co_return boost::leaf::new_error(http::Error_Version{std::move(http_version)});
    }
    if (status_code != 200)
    {
        co_return boost::leaf::new_error(http::Error_StatusCode{status_code});
    }

    LEAF_CO_CHECK(co_await asio::async_read_until(socket, response, "\r\n\r\n", co_leaf));

    std::istream headers_stream(&response);
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
    add_content(response);

    std::error_code ec;
    while (!ec)
    {
        co_await asio::async_read(socket, response,
            asio::transfer_at_least(1), asio::redirect_error(asio::use_awaitable, ec));
        add_content(response);
    }
    if (ec != asio::error::eof)
    {
        co_return boost::leaf::new_error(ec);
    }

    co_return std::move(content);
}
