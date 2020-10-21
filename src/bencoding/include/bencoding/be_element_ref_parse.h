#pragma once
#include <bencoding/be_element_ref.h>
#include <bencoding/be_errors.h>

// For outcome_throw_as_system_error_with_payload().
#include <cassert>
#include <cstdlib>

namespace be
{
    struct ParseErrorInfo
    {
        std::error_code ec;
        std::size_t position = 0u;
        ElementId element = ElementId::None;
    };
    
    // From:
    // https://ned14.github.io/outcome/tutorial/advanced/payload/copy_file2/.
    // Tell Outcome that ParseErrorInfo is to be treated as a std::error_code.
    inline const std::error_code& make_error_code(const ParseErrorInfo& ei)
    {
        return ei.ec;
    }

    [[noreturn]] inline void outcome_throw_as_system_error_with_payload(ParseErrorInfo ei)
    {
        (void)ei; assert(false);
        std::abort();
    }

    template<typename T>
    using Parsed = outcome::result<T, ParseErrorInfo>;

    Parsed<ListRef>       Parse(std::string_view bencoded);
    Parsed<StringRef>     ParseString(std::string_view bencoded);
    Parsed<IntegerRef>    ParseInteger(std::string_view bencoded);
    Parsed<ListRef>       ParseList(std::string_view bencoded);
    Parsed<DictionaryRef> ParseDictionary(std::string_view bencoded);

} // namespace be
