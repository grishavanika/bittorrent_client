#pragma once
#include "client_errors.h"
#include "utils_asio.h"

#include <bencoding/be_tracker_response_parse.h>

#include <small_utils/utils_bytes.h>

#include <string>
#include <variant>

#include <cstdint>

struct Tracker
{
    struct RequestInfo
    {
        std::uint16_t server_port = 0;
        std::uint32_t pieces_left = 0;
        std::uint32_t uploaded_pieces = 0;
        std::uint32_t downloaded_pieces = 0;
    };

    struct HTTP_GetRequest
    {
        std::string host_;
        std::uint16_t port_ = 0;
        std::string get_uri_;
    };

    struct HTTPS_GetRequest : HTTP_GetRequest {};

    struct UDP_Request : RequestInfo
    {
        std::string host_;
        std::uint16_t port_ = 0;
        SHA1Bytes info_hash_;
        PeerId peer_id_;
    };

    using Request =
        std::variant<std::monostate
            , HTTP_GetRequest
            , HTTPS_GetRequest
            , UDP_Request>;

    using AllTrackers = std::vector<Request>;
};

namespace be
{
    co_asio_result<TrackerResponse>
        HTTP_TrackerAnnounce(asio::io_context& io_context
            , const Tracker::HTTP_GetRequest& request);

    co_asio_result<TrackerResponse>
        HTTPS_TrackerAnnounce(asio::io_context& io_context
            , const Tracker::HTTPS_GetRequest& request);

    co_asio_result<TrackerResponse>
        UDP_TrackerAnnounce(asio::io_context& io_context
            , std::random_device& random
            , const Tracker::UDP_Request& request);
} // namespace be

