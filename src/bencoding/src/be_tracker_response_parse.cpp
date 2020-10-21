#include <bencoding/be_tracker_response_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_errors.h>
#include <small_utils/utils_string.h>

#include <cstring>

namespace be
{
    struct KeyParser
    {
        const char* const key_;
        outcome::result<void> (*parse_)(TrackerResponse& response, ElementRef& element);
        bool parsed_;
    };

    template<unsigned N>
    static outcome::result<void> InvokeParser(TrackerResponse& response
        , KeyParser(&parsers)[N]
        , std::string_view key
        , ElementRef& value)
    {
        for (KeyParser& state : parsers)
        {
            if ((state.parsed_) || (key != state.key_))
            {
                continue;
            }
            state.parsed_ = true;
            return state.parse_(response, value);
        }
        return outcome::failure(ParseTrackerErrorc::Impl_NoKeyFound);
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

    static outcome::result<void> ParseResponse_Interval(TrackerResponse& response, ElementRef& interval)
    {
        IntegerRef* n = interval.as_integer();
        if (!n)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnSuccess>(response);
        if (!state)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        if (!ParseLength(*n, state->rerequest_dt_secs_))
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        return outcome::success();
    }

    static outcome::result<void> ParseResponse_Peers(TrackerResponse& response, ElementRef& interval)
    {
        constexpr std::size_t k_packed_peer_size = 4/*ip v4*/ + 2/*port*/;
        static_assert(sizeof(PeerAddress::ipv4_) == 4);
        static_assert(sizeof(PeerAddress::port_) == 2);

        StringRef* peers_blob = interval.as_string();
        if (!peers_blob || peers_blob->empty())
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        if ((peers_blob->size() % k_packed_peer_size) != 0)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnSuccess>(response);
        if (!state)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        state->peers_.reserve(peers_blob->size() / k_packed_peer_size);
        const char* current = AsConstData(*peers_blob);
        const char* const end = current + peers_blob->size();
        while (current != end)
        {
            PeerAddress& peer = state->peers_.emplace_back(PeerAddress{});
            std::memcpy(&peer.ipv4_, current, sizeof(peer.ipv4_));
            current += sizeof(peer.ipv4_);
            std::memcpy(&peer.port_, current, sizeof(peer.port_));
            current += sizeof(peer.port_);
        }
        return outcome::success();
    }

    static outcome::result<void> ParseResponse_Failure(TrackerResponse& response, ElementRef& failure)
    {
        StringRef* str = failure.as_string();
        if (!str)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        auto state = GetOrCreateOnly<TrackerResponse::OnError>(response);
        if (!state)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        if (!str->empty())
        {
            state->error_.assign(AsConstData(*str), str->size());
        }
        return outcome::success();
    }

    outcome::result<TrackerResponse>
        ParseTrackerCompactResponseContent(std::string_view content)
    {
        OUTCOME_TRY(data, ParseDictionary(content));

        KeyParser k_parsers[] =
        {
            {"interval",       &ParseResponse_Interval, false},
            {"peers",          &ParseResponse_Peers,    false},
            {"failure reason", &ParseResponse_Failure,  false},
        };

        TrackerResponse response;
        for (auto& [name, element] : data)
        {
            auto result = InvokeParser(response, k_parsers, name, element);
            if (result)
            {
                continue;
            }
            if (result.error() == ParseTrackerErrorc::Impl_NoKeyFound)
            {
                // That's fine. Some of the keys are optional.
                // Final object's invariant will be validated at the end.
                continue;
            }
            return outcome::failure(result.error());
        }

        if (response.data_.index() == 0)
        {
            return outcome::failure(ParseTrackerErrorc::TODO);
        }
        return response;
    }
} // namespace be
