#pragma once
#include <nonstd/expected.hpp>

#include <variant>
#include <string_view>
#include <map>
#include <utility>
#include <vector>

#include <cassert>
#include <cctype>

namespace be
{
    enum class ElementId
    {
        None,
        Integer,
        String,
        List,
        Dictionary,
    };

    class ElementRef;
    using IntegerRef     = std::string_view;
    using StringRef      = std::string_view;
    using ListRef        = std::vector<ElementRef>;
    using DictionaryRef  = std::vector<std::pair<StringRef, ElementRef>>;
    using StorageRef     = std::variant<IntegerRef, StringRef, ListRef, DictionaryRef>;

    // Index of ElementId in `StorageRef` variant.
    constexpr std::size_t ElementIdToIndex(ElementId id)
    {
        switch (id)
        {
        case ElementId::Integer:    return 0u;
        case ElementId::String:     return 1u;
        case ElementId::List:       return 2u;
        case ElementId::Dictionary: return 3u;
        }
        return std::size_t(-1);
    }

    class ElementRef
    {
    public:
        explicit ElementRef();
        explicit ElementRef(StorageRef&& storage);

    public:
        ElementId element_id() const;

        bool is_valid() const;

        const StringRef* as_string() const;
        StringRef* as_string();

        const IntegerRef* as_integer() const;
        IntegerRef* as_integer();

        const ListRef* as_list() const;
        ListRef* as_list();

        const DictionaryRef* as_dictionary() const;
        DictionaryRef* as_dictionary();

    public:
        friend bool operator==(const ElementRef& lhs, const ElementRef& rhs);
        friend bool operator!=(const ElementRef& lhs, const ElementRef& rhs);

    private:
        StorageRef storage_;
    };

    /*explicit*/ inline ElementRef::ElementRef()
        : storage_()
    {
    }

    /*explicit*/ inline ElementRef::ElementRef(StorageRef&& storage)
        : storage_(std::move(storage))
    {
    }

    inline bool ElementRef::is_valid() const
    {
        return (element_id() != ElementId::None);
    }

    inline ElementId ElementRef::element_id() const
    {
        switch (storage_.index())
        {
        case ElementIdToIndex(ElementId::Integer):    return ElementId::Integer;
        case ElementIdToIndex(ElementId::String):     return ElementId::String;
        case ElementIdToIndex(ElementId::List):       return ElementId::List;
        case ElementIdToIndex(ElementId::Dictionary): return ElementId::Dictionary;
        }
        return ElementId::None;
    }

    inline const StringRef* ElementRef::as_string() const
    {
        return std::get_if<ElementIdToIndex(ElementId::String)>(&storage_);
    }

    inline StringRef* ElementRef::as_string()
    {
        return const_cast<StringRef*>(static_cast<const ElementRef&>(*this).as_string());
    }

    inline const IntegerRef* ElementRef::as_integer() const
    {
        return std::get_if<ElementIdToIndex(ElementId::Integer)>(&storage_);
    }

    inline IntegerRef* ElementRef::as_integer()
    {
        return const_cast<IntegerRef*>(static_cast<const ElementRef&>(*this).as_integer());
    }

    inline const ListRef* ElementRef::as_list() const
    {
        return std::get_if<ElementIdToIndex(ElementId::List)>(&storage_);
    }

    inline ListRef* ElementRef::as_list()
    {
        return const_cast<ListRef*>(static_cast<const ElementRef&>(*this).as_list());
    }

    inline const DictionaryRef* ElementRef::as_dictionary() const
    {
        return std::get_if<ElementIdToIndex(ElementId::Dictionary)>(&storage_);
    }

    inline DictionaryRef* ElementRef::as_dictionary()
    {
        return const_cast<DictionaryRef*>(static_cast<const ElementRef&>(*this).as_dictionary());
    }

    inline bool operator==(const ElementRef& lhs, const ElementRef& rhs)
    {
        return (lhs.storage_ == rhs.storage_);
    }

    inline bool operator!=(const ElementRef& lhs, const ElementRef& rhs)
    {
        return !(lhs == rhs);
    }

} // namespace be
