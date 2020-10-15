#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{
    class ListRefBuilder
    {
    public:
        ListRefBuilder& add(ElementRef&& element)
        {
            list_.push_back(std::move(element));
            return *this;
        }

        ElementRef build_once(ElementPosition p)
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::List)>();
            return ElementRef(StorageRef(tag, std::move(list_)), p);
        }

    private:
        ListRef list_;
    };

    class DictionaryRefBuilder
    {
    public:
        DictionaryRefBuilder& add(StringRef&& key,
            ElementRef&& value)
        {
            dict_.emplace_back(std::piecewise_construct,
                std::make_tuple(std::move(key)),
                std::make_tuple(std::move(value)));
            return *this;
        }

        ElementRef build_once(ElementPosition p)
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::Dictionary)>();
            return ElementRef(StorageRef(tag, std::move(dict_)), p);
        }

    private:
        DictionaryRef dict_;
    };

    class IntegerRefBuilder
    {
    public:
        IntegerRefBuilder& set(std::string_view number)
        {
            number_ = std::move(number);
            return *this;
        }

        ElementRef build_once(ElementPosition p)
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::Integer)>();
            return ElementRef(StorageRef(tag, std::move(number_)), p);
        }

    private:
        IntegerRef number_;
    };

    class StringRefBuilder
    {
    public:
        StringRefBuilder& set(std::string_view str)
        {
            str_ = std::move(str);
            return *this;
        }

        ElementRef build_once(ElementPosition p)
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::String)>();
            return ElementRef(StorageRef(tag, std::move(str_)), p);
        }

    private:
        StringRef str_;
    };

} // namespace be
