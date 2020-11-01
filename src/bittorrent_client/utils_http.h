#pragma once
#include "utils_outcome.h"
#include "utils_asio.h"

#include <string>

#include <cstdint>

co_asio_result<std::string>
    HTTP_GET(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n = 80);

co_asio_result<std::string>
    HTTPS_GET_NoVerification(asio::io_context& io_context
        , std::string host, std::string get_uri
        , std::uint16_t port_n = 443);
