#include "utils_http.h"
#include "client_errors.h"
#include "asio_outcome_as_result.hpp"

#include <asio/ssl.hpp>

#include <string>
#include <iostream>
#include <system_error>
#include <algorithm>
#include <iterator>

#include <cstdint>

namespace detail
{
    struct HTTPContext
    {
        asio::ip::tcp::socket socket_;
        HTTPContext(asio::io_context& io_context)
            : socket_(io_context) { }

        static HTTPContext make(asio::io_context& io_context)
        {
            return HTTPContext(io_context);
        }

        co_asio_result<void> connect(
            asio::ip::tcp::resolver::results_type& endpoints)
        {
            auto coro = as_result(asio::use_awaitable);
            OUTCOME_CO_TRY(co_await asio::async_connect(
                socket_, std::move(endpoints), coro));
            co_return outcome::success();
        }

        asio::ip::tcp::socket& get_socket()
        {
            return socket_;
        }
    };

    // From https://www.boost.org/doc/libs/1_74_0/doc/html/boost_asio/overview/ssl.html.
    // Also: https://dens.website/tutorials/cpp-asio/ssl-tls.
    using SSLStream = asio::ssl::stream<asio::ip::tcp::socket>;

    struct HTTPSContext
    {
        asio::ssl::context ssl_context_;
        SSLStream ssl_stream_;
        HTTPSContext(asio::io_context& io_context)
            : ssl_context_(asio::ssl::context::method::sslv23_client)
            , ssl_stream_(io_context, ssl_context_) { }

        static HTTPSContext make(asio::io_context& io_context)
        {
            return HTTPSContext(io_context);
        }

        co_asio_result<void> connect(
            asio::ip::tcp::resolver::results_type& endpoints)
        {
            auto coro = as_result(asio::use_awaitable);
            OUTCOME_CO_TRY(co_await asio::async_connect(
                ssl_stream_.lowest_layer(), std::move(endpoints), coro));
            OUTCOME_CO_TRY(co_await ssl_stream_.async_handshake(
                asio::ssl::stream_base::handshake_type::client, coro));
            co_return outcome::success();
        }

        SSLStream& get_socket()
        {
            return ssl_stream_;
        }
    };

    // From:
    // coroutines_ts/echo_server.cpp:
    // https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp17/coroutines_ts/echo_server.cpp
    // http/async_client.cpp:
    // https://github.com/chriskohlhoff/asio/blob/master/asio/src/examples/cpp03/http/client/async_client.cpp
    template<typename Context>
    static co_asio_result<std::string>
        HTTP_GET(asio::io_context& io_context
            , std::string host, std::string get_uri
            , std::uint16_t port_n)
    {
        auto context = Context::make(io_context);
        auto& socket = context.get_socket();
        asio::ip::tcp::resolver resolver(io_context);

        asio::streambuf request;
        asio::streambuf response;

        const std::string port = std::to_string(port_n);
        const std::string server = (host + ':' + port);

        std::ostream request_stream(&request);
        request_stream << "GET " << get_uri << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

        auto coro = as_result(asio::use_awaitable);

        OUTCOME_CO_TRY(auto endpoints, co_await resolver.async_resolve(host, port, coro));
        OUTCOME_CO_TRY(co_await context.connect(endpoints));
        OUTCOME_CO_TRY(co_await asio::async_write(socket, request, coro));
        OUTCOME_CO_TRY(co_await asio::async_read_until(socket, response, "\r\n", coro));

        // Check that response is OK.
        std::istream response_stream(&response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code = 0;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream
            || (http_version.substr(0, 5) != "HTTP/"))
        {
            co_return outcome::failure(ClientErrorc::TODO);
        }
        if (status_code != 200)
        {
            co_return outcome::failure(ClientErrorc::TODO);
        }

        OUTCOME_CO_TRY(co_await asio::async_read_until(socket, response, "\r\n\r\n", coro));

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

        while (true)
        {
            auto result = co_await asio::async_read(socket, response,
                asio::transfer_at_least(1), coro);
            if (result)
            {
                add_content(response);
                continue;
            }
            if (result.error() != asio::error::eof)
            {
                co_return outcome::failure(result.error());
            }
            break;
        }

        co_return outcome::success(std::move(content));
    }
} // namespace detail

co_asio_result<std::string>
    HTTP_GET(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n /*= 80*/)
{
    OUTCOME_CO_TRY(std::string data, co_await ::detail::HTTP_GET<detail::HTTPContext>(
        io_context, std::move(host), std::move(get_uri), port_n));
    co_return outcome::success(std::move(data));
}

co_asio_result<std::string>
    HTTPS_GET_NoVerification(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n /*= 443*/)
{
    OUTCOME_CO_TRY(std::string data, co_await ::detail::HTTP_GET<detail::HTTPSContext>(
        io_context, std::move(host), std::move(get_uri), port_n));
    co_return outcome::success(std::move(data));
}
