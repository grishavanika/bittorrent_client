#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{
    enum class DecodeErrorKind
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

    struct DecodeError
    {
        std::size_t pos = 0u;
        ElementId element = ElementId::None;
        DecodeErrorKind kind = DecodeErrorKind::Unknown;
    };

    template<typename T>
    using Decoded = nonstd::expected<T, DecodeError>;

    Decoded<ListRef> Decode(std::string_view bencoded);
    Decoded<StringRef> DecodeString(std::string_view bencoded);
    Decoded<IntegerRef> DecodeInteger(std::string_view bencoded);
    Decoded<ListRef> DecodeList(std::string_view bencoded);
    Decoded<DictionaryRef> DecodeDictionary(std::string_view bencoded);

} // namespace be
