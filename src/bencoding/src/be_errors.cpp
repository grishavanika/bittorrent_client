#include <bencoding/be_errors.h>

namespace
{
    struct ParseErrorCategory : std::error_category
    {
        virtual const char* name() const noexcept override;
        virtual std::string message(int ev) const override;
    };

    const char* ParseErrorCategory::name() const noexcept
    {
        return "be_parse";
    }

    std::string ParseErrorCategory::message(int ev) const
    {
        using E = be::ParseErrorc;
        switch (E(ev))
        {
        case E::Ok                       : return "<success>";
        case E::UnexpectedEnd            : return "unexpected end of parsing stream";
        case E::UnexpectedStringLength   : return "unexpected character for string length";
        case E::BadInteger               : return "invalid character for integer";
        case E::BadStringLength          : return "invalid string length";
        case E::StringOutOfBound         : return "mismatch in string length and available data";
        case E::NonStringAsDictionaryKey : return "non-string as key for dictionary";
        case E::MissingListStart         : return "missing list start";
        case E::MissingListEnd           : return "missing list end";
        case E::MissingDictionaryStart   : return "missing dictionary start";
        case E::MissingDictionaryEnd     : return "missing dictionary end";
        case E::MissingIntegerStart      : return "missing integer start";
        case E::MissingIntegerEnd        : return "missing integer end";
        case E::MissingStringStart       : return "missing string start";
        case E::NotString                : return "not a string as expected";
        case E::NotInteger               : return "not an integer as expected";
        case E::NotDictionary            : return "not a dictionary as expected";
        case E::NotList                  : return "not a list as expected";
        case E::InvalidInteger           : return "InvalidInteger";

        case E::InvalidInvariant              : return "InvalidInvariant";
        case E::EmptyAnnounce                 : return "EmptyAnnounce";
        case E::InvalidInfoPiecesLength20     : return "InvalidInfoPiecesLength20";
        case E::AmbiguousMultiOrSingleTorrent : return "AmbiguousMultiOrSingleTorrent";
        case E::EmptyMultiFileName            : return "EmptyMultiFileName";
        case E::MissingMultiFileProperty      : return "MissingMultiFileProperty";
        case E::EmptyMultiFile                : return "EmptyMultiFile";
        case E::MissingInfoProperty           : return "MissingInfoProperty";

        case E::Impl_InvalidInvariant         : return "Impl_InvalidInvariant";
        case E::InvalidPeersBlobLength        : return "InvalidPeersBlobLength";
        case E::MissingRequiredProperty       : return "MissingRequiredProperty";
        }
        return "<unknown>";
    }
} // namespace

namespace be
{
    std::error_code make_error_code(ParseErrorc e)
    {
        static const ParseErrorCategory domain;
        return {int(e), domain};
    }
} // namespace be
