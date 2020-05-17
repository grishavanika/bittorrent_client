#pragma once
#include <bencoding/be_element_ref.h>

namespace be
{
    class ListRefBuilder
    {
    public:
        ListRefBuilder& add(BEElementRef&& element)
        {
            list_.push_back(std::move(element));
            return *this;
        }

        BEElementRef build_once()
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::List)>();
            return BEElementRef(StorageRef(tag, std::move(list_)));
        }

    private:
        ListRef list_;
    };

    class DictionaryRefBuilder
    {
    public:
        DictionaryRefBuilder& add(StringRef&& key,
            BEElementRef&& value)
        {
            dict_.emplace_back(std::piecewise_construct,
                std::make_tuple(std::move(key)),
                std::make_tuple(std::move(value)));
            return *this;
        }

        BEElementRef build_once()
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::Dictionary)>();
            return BEElementRef(StorageRef(tag, std::move(dict_)));
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

        BEElementRef build_once()
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::Integer)>();
            return BEElementRef(StorageRef(tag, std::move(number_)));
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

        BEElementRef build_once()
        {
            constexpr auto tag = std::in_place_index_t<ElementIdToIndex(ElementId::String)>();
            return BEElementRef(StorageRef(tag, std::move(str_)));
        }

    private:
        StringRef str_;
    };

} // namespace be
