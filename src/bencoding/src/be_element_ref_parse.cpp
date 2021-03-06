#include <bencoding/be_element_ref_parse.h>
#include <bencoding/be_element_ref_builders.h>

#include <small_utils/utils_string.h>

#include <cassert>

namespace be
{
    constexpr char k_integer_start    = 'i';
    constexpr char k_list_start       = 'l';
    constexpr char k_dictionary_start = 'd';
    constexpr char k_element_end      = 'e';
    constexpr char k_string_start     = ':';

    using ParsedElementRef = outcome::result<ElementRef, ParseErrorInfo>;

    struct Parser
    {
        Parser(const char* start, std::size_t length)
            : current_(start)
            , end_(start + length)
            , start_(start)
        {
        }

        struct RememberPosition
        {
            RememberPosition(const Parser& self) noexcept
                : self_(self)
                , start_(self.current_)
            {
                assert(start_ >= self_.start_);
            }

            ElementPosition get() const
            {
                assert(start_ >= self_.start_);
                assert(self_.current_ >= self_.start_);
                ElementPosition p;
                p.start_ = (start_ - self_.start_);
                p.end_   = (self_.current_ - self_.start_);
                return p;
            }

            RememberPosition(const RememberPosition&) = delete;
            RememberPosition(RememberPosition&&) = delete;
            RememberPosition& operator=(const RememberPosition&) = delete;
            RememberPosition& operator=(RememberPosition&&) = delete;

        private:
            const Parser& self_;
            const char* const start_;
        };

        ParsedElementRef parse_element(ElementId parent_id = ElementId::None)
        {
            if (current_ >= end_)
            {
                return make_error(parent_id, ParseErrorc::UnexpectedEnd);
            }
            switch (*current_)
            {
            case k_integer_start    : return parse_integer();
            case k_list_start       : return parse_list();
            case k_dictionary_start : return parse_dictionary();
            default                 : return parse_string();
            }
        }

        ParsedElementRef parse_integer()
        {
            const RememberPosition position(*this);

            if (!consume(k_integer_start))
            {
                return make_error(ElementId::Integer, ParseErrorc::MissingIntegerStart);
            }
            const char* const begin = current_;
            const bool has_sign = (has_data() && (*begin == '-'));
            if (has_sign)
            {
                ++current_;
            }

            while (has_data() && (*current_ != k_element_end))
            {
                if (std::isdigit(*current_))
                {
                    ++current_;
                }
                else
                {
                    return make_error(ElementId::Integer, ParseErrorc::BadInteger);
                }
            }

            const char* const last = current_;
            if (!consume(k_element_end))
            {
                return make_error(ElementId::Integer, ParseErrorc::BadInteger);
            }

            if (begin == last)
            {
                // Missing number's digits: "ie".
                return make_error(ElementId::Integer, ParseErrorc::BadInteger);
            }
            else if (has_sign && (last == (begin + 1)))
            {
                // Only `-` was specified: "i-e".
                return make_error(ElementId::Integer, ParseErrorc::BadInteger);
            }
            else if (has_sign && *(begin + 1) == '0')
            {
                // "i-0e" case.
                return make_error(ElementId::Integer, ParseErrorc::BadInteger);
            }
            else if ((*begin == '0') && (last > (begin + 1)))
            {
                // "i03e" case.
                return make_error(ElementId::Integer, ParseErrorc::BadInteger);
            }
            return IntegerRefBuilder()
                .set(std::string_view(begin, last - begin))
                .build_once(position.get());
        }

        ParsedElementRef parse_list()
        {
            const RememberPosition position(*this);

            if (!consume(k_list_start))
            {
                return make_error(ElementId::String, ParseErrorc::MissingListStart);
            }
            ListRefBuilder builder;
            while (has_data() && (*current_ != k_element_end))
            {
                auto element = parse_element(ElementId::List);
                if (!element)
                {
                    return element;
                }
                builder.add(std::move(element.value()));
            }
            if (!consume(k_element_end))
            {
                return make_error(ElementId::String, ParseErrorc::MissingListEnd);
            }
            return builder.build_once(position.get());
        }

        ParsedElementRef parse_dictionary()
        {
            const RememberPosition position(*this);

            if (!consume(k_dictionary_start))
            {
                return make_error(ElementId::Dictionary, ParseErrorc::MissingDictionaryStart);
            }
            DictionaryRefBuilder builder;
            while (has_data() && (*current_ != k_element_end))
            {
                OUTCOME_TRY(key, parse_element(ElementId::Dictionary));
                if (!key.as_string())
                {
                    return make_error(ElementId::Dictionary, ParseErrorc::NonStringAsDictionaryKey);
                }
                OUTCOME_TRY(value, parse_element(ElementId::Dictionary));
                builder.add(std::move(*key.as_string()), std::move(value));
            }
            if (!consume(k_element_end))
            {
                return make_error(ElementId::Dictionary, ParseErrorc::MissingDictionaryEnd);
            }
            return builder.build_once(position.get());
        }

        ParsedElementRef parse_string()
        {
            const RememberPosition position(*this);

            OUTCOME_TRY(length_element, parse_string_length());
            assert(length_element.as_integer());

            std::uint64_t length = 0;
            if (!ParseLength(*length_element.as_integer(), length))
            {
                return make_error(ElementId::String, ParseErrorc::BadStringLength);
            }

            const char* const begin = current_;
            if (!consume_n(length))
            {
                return make_error(ElementId::String, ParseErrorc::StringOutOfBound);
            }
            return StringRefBuilder()
                .set(std::string_view(begin, std::size_t(length)))
                .build_once(position.get());
        }

        [[nodiscard]] bool has_data() const
        {
            return (current_ < end_);
        }

    private:
        ParsedElementRef make_error(ElementId element, ParseErrorc e) const
        {
            ParseErrorInfo error;
            error.position = (current_ - start_);
            error.element = element;
            error.ec = e;
            return outcome::failure(error);
        }

        [[nodiscard]] bool consume(char ch)
        {
            if (has_data() && (*current_ == ch))
            {
                ++current_;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool consume_n(std::size_t count)
        {
            if ((start_ + count) < end_)
            {
                current_ += count;
                return true;
            }
            return false;
        }

        ParsedElementRef parse_string_length()
        {
            const RememberPosition position(*this);

            const char* begin = current_;
            while (has_data())
            {
                if (std::isdigit(*current_))
                {
                    ++current_;
                    continue;
                }
                else if (*current_ == k_string_start)
                {
                    break;
                }
                else
                {
                    return make_error(ElementId::String, ParseErrorc::UnexpectedStringLength);
                }
            }

            if (begin == current_)
            {
                // Missing number's digits: ":str" or empty string "".
                return make_error(ElementId::String, ParseErrorc::UnexpectedStringLength);
            }

            if (!consume(k_string_start))
            {
                return make_error(ElementId::String, ParseErrorc::MissingStringStart);
            }

            return IntegerRefBuilder()
                .set(std::string_view(begin, current_ - begin - 1))
                .build_once(position.get());
        }

    private:
        const char* current_;
        const char* end_;
        const char* start_;
    };

    Parsed<ListRef> Parse(std::string_view bencoded)
    {
        Parser decoder(bencoded.data(), bencoded.size());

        ListRef elements;
        do
        {
            OUTCOME_TRY(element, decoder.parse_element());
            elements.push_back(std::move(element));
        }
        while (decoder.has_data());

        return Parsed<ListRef>(std::move(elements));
    }

    Parsed<StringRef> ParseString(std::string_view bencoded)
    {
        Parser decoder(bencoded.data(), bencoded.size());
        OUTCOME_TRY(element, decoder.parse_string());
        assert(element.as_string());
        return std::move(*element.as_string());
    }

    Parsed<IntegerRef> ParseInteger(std::string_view bencoded)
    {
        Parser decoder(bencoded.data(), bencoded.size());
        OUTCOME_TRY(element, decoder.parse_integer());
        assert(element.as_integer());
        return std::move(*element.as_integer());
    }

    Parsed<ListRef> ParseList(std::string_view bencoded)
    {
        Parser decoder(bencoded.data(), bencoded.size());
        OUTCOME_TRY(element, decoder.parse_list());
        assert(element.as_list());
        return std::move(*element.as_list());
    }

    Parsed<DictionaryRef> ParseDictionary(std::string_view bencoded)
    {
        Parser decoder(bencoded.data(), bencoded.size());
        OUTCOME_TRY(element, decoder.parse_dictionary());
        assert(element.as_dictionary());
        return std::move(*element.as_dictionary());
    }

} // namespace be
