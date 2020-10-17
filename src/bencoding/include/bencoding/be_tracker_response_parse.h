#pragma once
#include <variant>
#include <optional>
#include <vector>
#include <string>
#include <cstdint>

namespace be
{

    struct PeerAddress
    {
        // Network (big-endian) order.
        std::uint32_t ipv4_ = 0;
        std::uint16_t port_ = 0;
    };

    // Compact form.
    struct TrackerResponse
    {
        struct OnSuccess
        {
            std::uint64_t rerequest_dt_secs_ = 0;
            std::vector<PeerAddress> peers_;
        };
        struct OnError
        {
            std::string error_; // 'failure reason'.
        };

        std::variant<std::monostate
            , OnSuccess
            , OnError> data_;
    };

    // https://www.bittorrent.org/beps/bep_0003.html
    // https://www.bittorrent.org/beps/bep_0023.html
    std::optional<TrackerResponse> ParseTrackerCompactResponseContent(
        std::string_view content);
} // namespace be
