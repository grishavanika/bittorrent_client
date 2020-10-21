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
        case E::Ok:                       return "<success>";
        case E::UnexpectedEnd:            return "unexpected end of parsing stream";
        case E::UnexpectedStringLength:   return "unexpected character for string length";
        case E::BadInteger:               return "invalid character for integer";
        case E::BadStringLength:          return "invalid string length";
        case E::StringOutOfBound:         return "mismatch in string length and available data";
        case E::NonStringAsDictionaryKey: return "non-string as key for dictionary";
        case E::MissingListStart:         return "missing list start";
        case E::MissingListEnd:           return "missing list end";
        case E::MissingDictionaryStart:   return "missing dictionary start";
        case E::MissingDictionaryEnd:     return "missing dictionary end";
        case E::MissingIntegerStart:      return "missing integer start";
        case E::MissingIntegerEnd:        return "missing integer end";
        case E::MissingStringStart:       return "missing string start";
        }
        return "<unknown>";
    }

    struct ParseTorrentErrorCategory : std::error_category
    {
        virtual const char* name() const noexcept override;
        virtual std::string message(int ev) const override;
    };

    const char* ParseTorrentErrorCategory::name() const noexcept
    {
        return "be_parse_torrent";
    }

    std::string ParseTorrentErrorCategory::message(int ev) const
    {
        using E = be::ParseTorrentErrorc;
        switch (E(ev))
        {
        case E::Ok: return "<success>";
        default: break;
        }
        return "<unknown>";
    }

    struct ParseTrackerErrorCategory : std::error_category
    {
        virtual const char* name() const noexcept override;
        virtual std::string message(int ev) const override;
    };

    const char* ParseTrackerErrorCategory::name() const noexcept
    {
        return "be_parse_tracker";
    }

    std::string ParseTrackerErrorCategory::message(int ev) const
    {
        using E = be::ParseTrackerErrorc;
        switch (E(ev))
        {
        case E::Ok: return "<success>";
        default: break;
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

    std::error_code make_error_code(ParseTorrentErrorc e)
    {
        static const ParseTorrentErrorCategory domain;
        return {int(e), domain};
    }

    std::error_code make_error_code(ParseTrackerErrorc e)
    {
        static const ParseTrackerErrorCategory domain;
        return {int(e), domain};
    }
} // namespace be
