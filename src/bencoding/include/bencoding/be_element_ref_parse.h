#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{
    enum class ParseErrorKind
    {
        Unknown,
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

    struct ParseError
    {
        std::size_t position = 0u;
        ElementId element = ElementId::None;
        ParseErrorKind kind = ParseErrorKind::Unknown;
    };

    template<typename T>
    using Parsed = nonstd::expected<T, ParseError>;

    Parsed<ListRef>       Parse(std::string_view bencoded);
    Parsed<StringRef>     ParseString(std::string_view bencoded);
    Parsed<IntegerRef>    ParseInteger(std::string_view bencoded);
    Parsed<ListRef>       ParseList(std::string_view bencoded);
    Parsed<DictionaryRef> ParseDictionary(std::string_view bencoded);

} // namespace be
