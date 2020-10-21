#pragma once
#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;

#include <system_error>

namespace be
{
    // #UUU: instead of 3 different error enums,
    // one error list should be used with std::error_condition
    // implementation.
    // IFF we decide those 3 parts should be in one library.
    enum class ParseErrorc : int;
    std::error_code make_error_code(ParseErrorc);

    enum class ParseTorrentErrorc : int;
    std::error_code make_error_code(ParseTorrentErrorc);

    enum class ParseTrackerErrorc : int;
    std::error_code make_error_code(ParseTrackerErrorc);
} // namespace be

namespace std
{
    template <>
    struct is_error_code_enum<be::ParseErrorc> : true_type {};

    template <>
    struct is_error_code_enum<be::ParseTorrentErrorc> : true_type {};

    template <>
    struct is_error_code_enum<be::ParseTrackerErrorc> : true_type {};
} // namespace std

namespace be
{
    enum class ParseErrorc : int
    {
        Ok = 0,
        UnexpectedEnd,
        UnexpectedStringLength,
        BadInteger,
        BadStringLength,
        StringOutOfBound,
        NonStringAsDictionaryKey,
        MissingListStart,
        MissingListEnd,
        MissingDictionaryStart,
        MissingDictionaryEnd,
        MissingIntegerStart,
        MissingIntegerEnd,
        MissingStringStart,
    };

    enum class ParseTorrentErrorc : int
    {
        Ok = 0,
        Impl_NoKeyFound,
        TODO,
        InvalidInfoKeyType,
        InvalidInvariant,
        EmptyAnnounce,
        InvalidAnnounceType,
        InvalidInfoNameType,
        InvalidInfoPieceLengthType,
        InvalidInfoPieceLengthValue,
        InvalidInfoPiecesType,
        EmptyInfoPieces,
        InvalidInfoPiecesLength20,
        AmbiguousMultiOrSingleTorrent,
        InvalidInfoLengthType,
        InvalidInfoLengthValue,
    };

    enum class ParseTrackerErrorc : int
    {
        Ok = 0,
        Impl_NoKeyFound,
        TODO,
    };
} // namespace be
