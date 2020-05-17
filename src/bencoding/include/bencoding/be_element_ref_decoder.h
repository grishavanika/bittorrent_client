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

    Decoded<BEElementsArrayRef> Decode(std::string_view bencoded);
    Decoded<BEElementRef::String> DecodeString(std::string_view bencoded);
    Decoded<BEElementRef::Integer> DecodeInteger(std::string_view bencoded);
    Decoded<BEElementRef::List> DecodeList(std::string_view bencoded);
    Decoded<BEElementRef::Dictionary> DecodeDictionary(
        std::string_view bencoded);

} // namespace be
