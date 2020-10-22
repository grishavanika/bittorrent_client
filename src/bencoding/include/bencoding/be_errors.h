#pragma once
#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;

#include <system_error>

namespace be
{
    // #QQQ: add std::error_condition to differentiate
    // between categories we have.
    enum class ParseErrorc : int;
    std::error_code make_error_code(ParseErrorc);
} // namespace be

namespace std
{
    template <>
    struct is_error_code_enum<be::ParseErrorc> : true_type {};
} // namespace std

namespace be
{
    enum class ParseErrorc : int
    {
        Ok = 0,
        UnexpectedEnd = 100, // BE format parsing errors.
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
        NotString,
        NotInteger,
        NotDictionary,
        NotList,
        InvalidInteger,

        InvalidInvariant = 200, // .torrent file parsing errors.
        EmptyAnnounce,
        InvalidInfoPiecesLength20,
        AmbiguousMultiOrSingleTorrent,
        EmptyMultiFileName,
        MissingMultiFileProperty,
        EmptyMultiFile,
        MissingInfoProperty,

        Impl_InvalidInvariant = 300, // Tracker response parsing errors.
        InvalidPeersBlobLength,
        MissingRequiredProperty,
    };
} // namespace be
