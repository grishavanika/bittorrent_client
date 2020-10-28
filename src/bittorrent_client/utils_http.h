#pragma once
#include "utils_outcome.h"

#include <asio/awaitable.hpp>

#include <string>

#include <cstdint>

asio::awaitable<outcome::result<std::string>>
    HTTP_GET(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n = 80);

asio::awaitable<outcome::result<std::string>>
    HTTPS_GET_NoVerification(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n = 443);
