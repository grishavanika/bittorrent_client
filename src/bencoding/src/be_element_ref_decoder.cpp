#include <bencoding/be_element_ref_builders.h>
#include <bencoding/be_element_ref_decoder.h>

#if __has_include(<charconv>)
#include <charconv>
#define BE_HAS_FROM_CHARS() 1
#else
#include <cstdlib>
#define BE_HAS_FROM_CHARS() 0
#endif

#include <cassert>

namespace be
{
    namespace
    {
        constexpr char k_integer_start = 'i';
        constexpr char k_list_start = 'l';
        constexpr char k_dictionary_start = 'd';
        constexpr char k_element_end = 'e';
        constexpr char k_string_start = ':';

        bool ParseLength(std::string_view str, std::size_t& length)
        {
#if (BE_HAS_FROM_CHARS())
            const char* begin = str.data();
            const char* end = begin + str.size();
            const auto result = std::from_chars(begin, end, length, 10);
            if ((result.ptr != end) || (result.ec != std::errc()))
            {
                return false;
            }
            return true;
#else
            std::string temp(str.begin(), str.end());
            const char* begin = temp.c_str();
            const char* end = begin + temp.size();
            char* parse_end = nullptr;
            const long v = std::strtol(begin, &parse_end, 10);
            if ((begin != end) && (parse_end == end))
            {
                length = static_cast<std::size_t>(v);
                return true;
            }
            return false;
#endif
        }

        using DecodedElementRef = nonstd::expected<ElementRef, DecodeError>;

        struct Decoder
        {
            Decoder(const char* start, std::size_t length)
                : current_(start)
                , end_(start + length)
                , start_(start)
            {
            }

            DecodedElementRef decode_element(
                ElementId parent_id = ElementId::None)
            {
                if (current_ >= end_)
                {
                    return make_error(parent_id, DecodeErrorKind::UnexpectedEnd);
                }
                switch (*current_)
                {
                case k_integer_start: return decode_integer();
                case k_list_start: return decode_list();
                case k_dictionary_start: return decode_dictionary();
                default: return decode_string();
                }
            }

            DecodedElementRef decode_integer()
            {
                if (!consume(k_integer_start))
                {
                    return make_error(ElementId::Integer, DecodeErrorKind::MissingIntegerStart);
                }
                const char* const begin = current_;
                const bool sign = (has_data() && (*begin == '-'));
                if (sign)
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
                        return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                    }
                }

                const char* const last = current_;
                if (!consume(k_element_end))
                {
                    return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                }

                if (begin == last)
                {
                    // Missing number's digits: "ie"
                    return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                }
                else if (sign && (last == (begin + 1)))
                {
                    // Only `-` was specified: "i-e"
                    return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                }
                else if (sign && *(begin + 1) == '0')
                {
                    // "i-0e" case
                    return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                }
                else if ((*begin == '0') && (last > (begin + 1)))
                {
                    // "i03e" case
                    return make_error(ElementId::Integer, DecodeErrorKind::BadInteger);
                }
                return IntegerRefBuilder()
                    .set(std::string_view(begin, last - begin))
                    .build_once();
            }

            DecodedElementRef decode_list()
            {
                if (!consume(k_list_start))
                {
                    return make_error(ElementId::String, DecodeErrorKind::MissingListStart);
                }
                ListRefBuilder builder;
                while (has_data() && (*current_ != k_element_end))
                {
                    auto element = decode_element(ElementId::List);
                    if (!element)
                    {
                        return element;
                    }
                    builder.add(std::move(*element));
                }
                if (!consume(k_element_end))
                {
                    return make_error(ElementId::String, DecodeErrorKind::MissingListEnd);
                }
                return builder.build_once();
            }

            DecodedElementRef decode_dictionary()
            {
                if (!consume(k_dictionary_start))
                {
                    return make_error(ElementId::Dictionary, DecodeErrorKind::MissingDictionaryStart);
                }
                DictionaryRefBuilder builder;
                while (has_data() && (*current_ != k_element_end))
                {
                    auto key = decode_element(ElementId::Dictionary);
                    if (!key)
                    {
                        return key;
                    }
                    if (!key->as_string())
                    {
                        return make_error(ElementId::Dictionary, DecodeErrorKind::NonStringAsDictionaryKey);
                    }
                    auto value = decode_element(ElementId::Dictionary);
                    if (!value)
                    {
                        return value;
                    }
                    builder.add(std::move(*key->as_string()), std::move(*value));
                }
                if (!consume(k_element_end))
                {
                    return make_error(ElementId::Dictionary, DecodeErrorKind::MissingDictionaryEnd);
                }
                return builder.build_once();
            }

            DecodedElementRef decode_string()
            {
                auto length_element = decode_string_length();
                if (!length_element)
                {
                    return length_element;
                }
                assert(length_element->as_integer());

                std::size_t length = 0;
                if (!ParseLength(*length_element->as_integer(), length))
                {
                    return make_error(ElementId::String, DecodeErrorKind::BadStringLength);
                }

                const char* const begin = current_;
                if (!consume(length))
                {
                    return make_error(ElementId::String, DecodeErrorKind::StringOutOfBound);
                }
                return StringRefBuilder()
                    .set(std::string_view(begin, length))
                    .build_once();
            }

            [[nodiscard]] bool has_data() const
            {
                return (current_ < end_);
            }

        private:
            DecodedElementRef make_error(ElementId element, DecodeErrorKind kind) const
            {
                DecodeError error;
                error.pos = (current_ - start_);
                error.element = element;
                error.kind = kind;
                return nonstd::make_unexpected(std::move(error));
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

            [[nodiscard]] bool consume(std::size_t count)
            {
                if ((start_ + count) < end_)
                {
                    current_ += count;
                    return true;
                }
                return false;
            }

            DecodedElementRef decode_string_length()
            {
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
                        return make_error(ElementId::String, DecodeErrorKind::UnexpectedStringLength);
                    }
                }

                if (begin == current_)
                {
                    // Missing number's digits: ":str" or empty string ""
                    return make_error(ElementId::String, DecodeErrorKind::UnexpectedStringLength);
                }

                if (!consume(k_string_start))
                {
                    return make_error(ElementId::String, DecodeErrorKind::MissingStringStart);
                }

                return IntegerRefBuilder()
                    .set(std::string_view(begin, current_ - begin - 1))
                    .build_once();
            }

        private:
            const char* current_;
            const char* end_;
            const char* start_;
        };
    } // namespace

    Decoded<ListRef> Decode(std::string_view bencoded)
    {
        Decoder decoder(bencoded.data(), bencoded.size());

        ListRef elements;
        do
        {
            auto element = decoder.decode_element();
            if (element)
            {
                elements.push_back(std::move(*element));
                continue;
            }
            return nonstd::make_unexpected(std::move(element).error());
        }
        while (decoder.has_data());

        return elements;
    }

    Decoded<StringRef> DecodeString(std::string_view bencoded)
    {
        Decoder decoder(bencoded.data(), bencoded.size());
        auto element = decoder.decode_string();
        if (element)
        {
            assert(element->as_string());
            return std::move(*element->as_string());
        }
        return nonstd::make_unexpected(element.error());
    }

    Decoded<IntegerRef> DecodeInteger(std::string_view bencoded)
    {
        Decoder decoder(bencoded.data(), bencoded.size());
        auto element = decoder.decode_integer();
        if (element)
        {
            assert(element->as_integer());
            return std::move(*element->as_integer());
        }
        return nonstd::make_unexpected(element.error());
    }

    Decoded<ListRef> DecodeList(std::string_view bencoded)
    {
        Decoder decoder(bencoded.data(), bencoded.size());
        auto element = decoder.decode_list();
        if (element)
        {
            assert(element->as_list());
            return std::move(*element->as_list());
        }
        return nonstd::make_unexpected(element.error());
    }

    Decoded<DictionaryRef> DecodeDictionary(std::string_view bencoded)
    {
        Decoder decoder(bencoded.data(), bencoded.size());
        auto element = decoder.decode_dictionary();
        if (element)
        {
            assert(element->as_dictionary());
            return std::move(*element->as_dictionary());
        }
        return nonstd::make_unexpected(element.error());
    }

} // namespace be
