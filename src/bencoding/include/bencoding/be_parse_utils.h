#pragma once
#include <bencoding/be_element_ref.h>
#include <bencoding/be_errors.h>

#include <small_utils/utils_string.h>

#include <type_traits>

namespace be
{
    template<typename T>
    outcome::result<const T*> ElementRefAs(const ElementRef& e);
    template<typename T>
    outcome::result<T*> ElementRefAs(ElementRef& e);

    outcome::result<std::uint64_t> ParseAsUint64(const std::string_view& n);

    template<typename T>
    outcome::result<const T*> ElementRefAs(const ElementRef& e)
    {
        if constexpr (std::is_same_v<StringRef, T>)
        {
            if (auto v = e.as_string())
            {
                return outcome::success(v);
            }
            return outcome::failure(ParseErrorc::NotString);
        }
        else if constexpr (std::is_same_v<IntegerRef, T>)
        {
            if (auto v = e.as_integer())
            {
                return outcome::success(v);
            }
            return outcome::failure(ParseErrorc::NotInteger);
        }
        else if constexpr (std::is_same_v<ListRef, T>)
        {
            if (auto v = e.as_list())
            {
                return outcome::success(v);
            }
            return outcome::failure(ParseErrorc::NotList);
        }
        else if constexpr (std::is_same_v<DictionaryRef, T>)
        {
            if (auto v = e.as_dictionary())
            {
                return outcome::success(v);
            }
            return outcome::failure(ParseErrorc::NotDictionary);
        }
    }

    template<typename T>
    outcome::result<T*> ElementRefAs(ElementRef& e)
    {
        outcome::result<const T*> r = ElementRefAs<T>(const_cast<const ElementRef&>(e));
        if (r)
        {
            return outcome::success(const_cast<T*>(r.value()));
        }
        return outcome::failure(r.error());
    }

    inline outcome::result<std::uint64_t> ParseAsUint64(const std::string_view& n)
    {
        std::uint64_t v = 0;
        if (ParseLength(n, v))
        {
            return outcome::success(v);
        }
        return outcome::failure(ParseErrorc::InvalidInteger);
    }
} // namespace be
