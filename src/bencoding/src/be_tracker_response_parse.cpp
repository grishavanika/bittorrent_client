#include <bencoding/be_tracker_response_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <small_utils/utils_string.h>

#include <cstring>

namespace be
{
    struct KeyParser
    {
        const char* const key_;
        bool (*parse_)(TrackerResponse& response, ElementRef& element);
        bool parsed_;
    };

    template<unsigned N>
    static bool InvokeParser(TrackerResponse& response
        , KeyParser(&parsers)[N]
        , std::string_view key
        , ElementRef& value
        , bool& ok)
    {
        for (KeyParser& state : parsers)
        {
            if ((state.parsed_) || (key != state.key_))
            {
                continue;
            }
            state.parsed_ = true;
            ok = state.parse_(response, value);
            return true;
        }
        return false;
    }

    template<typename T>
    static T* GetOrCreateOnly(TrackerResponse& response)
    {
        if (T* data = std::get_if<T>(&response.data_))
        {
            return data;
        }
        if (response.data_.index() != 0) // std::monostate
        {
            return nullptr;
        }
        return &response.data_.emplace<T>();
    }

    static bool ParseResponse_Interval(TrackerResponse& response, ElementRef& interval)
    {
        IntegerRef* n = interval.as_integer();
        if (!n)
        {
            return false;
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnSuccess>(response);
        if (!state)
        {
            return false;
        }
        if (!ParseLength(*n, state->rerequest_dt_secs_))
        {
            return false;
        }
        return true;
    }

    static bool ParseResponse_Peers(TrackerResponse& response, ElementRef& interval)
    {
        constexpr std::size_t k_packed_peer_size = 4/*ip v4*/ + 2/*port*/;
        static_assert(sizeof(PeerInfo::ipv4_) == 4);
        static_assert(sizeof(PeerInfo::port_) == 2);

        StringRef* peers_blob = interval.as_string();
        if (!peers_blob)
        {
            return false;
        }
        if ((peers_blob->size() % k_packed_peer_size) != 0)
        {
            return false;
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnSuccess>(response);
        if (!state)
        {
            return false;
        }
        state->peers_.reserve(peers_blob->size() / k_packed_peer_size);
        const char* current = AsConstData(*peers_blob);
        const char* const end = current + peers_blob->size();
        while (current != end)
        {
            state->peers_.emplace_back(PeerInfo{});
            PeerInfo& peer = state->peers_.back();
            std::memcpy(&peer.ipv4_, current, sizeof(peer.ipv4_));
            current += sizeof(peer.ipv4_);
            std::memcpy(&peer.port_, current, sizeof(peer.port_));
            current += sizeof(peer.port_);
        }
        return true;
    }

    static bool ParseResponse_Failure(TrackerResponse& response, ElementRef& failure)
    {
        StringRef* str = failure.as_string();
        if (!str)
        {
            return false;
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnError>(response);
        if (!state)
        {
            return false;
        }
        if (!str->empty())
        {
            state->error_.assign(AsConstData(*str), str->size());
        }
        return true;
    }

    std::optional<TrackerResponse> ParseTrackerCompactResponseContent(
        std::string_view content)
    {
        Parsed<DictionaryRef> data = ParseDictionary(content);
        if (!data.has_value())
        {
            return std::nullopt;
        }

        KeyParser k_parsers[] =
        {
            {"interval",       &ParseResponse_Interval, false},
            {"peers",          &ParseResponse_Peers,    false},
            {"failure reason", &ParseResponse_Failure,  false},
        };

        std::optional<TrackerResponse> response(std::in_place);
        for (auto& [name, element] : *data)
        {
            bool ok = false;
            if (InvokeParser(*response, k_parsers, name, element, ok)
                && !ok)
            {
                return std::nullopt;
            }
        }
        if (response->data_.index() == 0)
        {
            // Nothing is emplaced.
            return std::nullopt;
        }
        return response;
    }
} // namespace be
