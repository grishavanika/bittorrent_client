#include <bencoding/be_tracker_response_parse.h>
#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_errors.h>
#include <bencoding/be_parse_utils.h>

#include <small_utils/utils_string.h>

#include <cstring>

namespace be
{
    constexpr std::size_t k_packed_peer_size = 4/*ip v4*/ + 2/*port*/;
    static_assert(sizeof(PeerAddress::ipv4_) == 4);
    static_assert(sizeof(PeerAddress::port_) == 2);

    struct KeyParser
    {
        const char* const key_;
        outcome::result<void> (*parse_)(TrackerResponse& response, ElementRef& element);
        bool parsed_;
    };

    template<unsigned N>
    static outcome::result<void> InvokeParserOptionalKey(TrackerResponse& response
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
        return outcome::success();
    }

    template<typename T>
    static outcome::result<T*> GetOrCreateOnly(TrackerResponse& response)
    {
        if (T* data = std::get_if<T>(&response.data_))
        {
            return outcome::success(data);
        }
        if (auto data = std::get_if<std::monostate>(&response.data_))
        {
            // Nothing yet constructed.
            return outcome::success(&response.data_.emplace<T>());
        }
        return outcome::failure(ParseErrorc::Impl_InvalidInvariant);
    }

    static outcome::result<void> ParseResponse_Interval(TrackerResponse& response, ElementRef& interval)
    {
        OUTCOME_TRY(n, be::ElementRefAs<IntegerRef>(interval));
        OUTCOME_TRY(v, ParseAsUint64(*n));
        OUTCOME_TRY(state, GetOrCreateOnly<TrackerResponse::OnSuccess>(response));
        state->rerequest_dt_secs_ = v;
        return outcome::success();
    }

    static outcome::result<void> ParseResponse_Peers(TrackerResponse& response, ElementRef& interval)
    {
        OUTCOME_TRY(peers_blob, be::ElementRefAs<StringRef>(interval));
        if (peers_blob->empty() || ((peers_blob->size() % k_packed_peer_size) != 0))
        {
            return outcome::failure(ParseErrorc::InvalidPeersBlobLength);
        }
        OUTCOME_TRY(state, GetOrCreateOnly<TrackerResponse::OnSuccess>(response));
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
        OUTCOME_TRY(str, be::ElementRefAs<StringRef>(failure));
        OUTCOME_TRY(state, GetOrCreateOnly<TrackerResponse::OnError>(response));
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
            OUTCOME_TRY(InvokeParserOptionalKey(response, k_parsers, name, element));
        }

        if (response.data_.index() == 0)
        {
            return outcome::failure(ParseErrorc::MissingRequiredProperty);
        }
        return response;
    }
} // namespace be
