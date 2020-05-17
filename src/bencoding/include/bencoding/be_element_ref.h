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

    class BEElementRef;
    using BEElementsArrayRef = std::vector<BEElementRef>;
    using IntegerRef         = std::string_view;
    using StringRef          = std::string_view;
    using ListRef            = std::vector<BEElementRef>;
    using DictionaryRef      = std::vector<std::pair<StringRef, BEElementRef>>;
    using StorageRef         = std::variant<IntegerRef, StringRef, ListRef, DictionaryRef>;

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

    class BEElementRef
    {
    public:
        explicit BEElementRef();
        explicit BEElementRef(StorageRef&& storage);

    public:
        ElementId element_id() const;

        bool is_valid() const;

        bool is_string() const;
        const StringRef& as_string() const;
        StringRef& as_string();

        bool is_integer() const;
        const IntegerRef& as_integer() const;
        IntegerRef& as_integer();

        bool is_list() const;
        const ListRef& as_list() const;
        ListRef& as_list();

        bool is_dictionary() const;
        const DictionaryRef& as_dictionary() const;
        DictionaryRef& as_dictionary();

    public:
        friend bool operator==(const BEElementRef& lhs, const BEElementRef& rhs);
        friend bool operator!=(const BEElementRef& lhs, const BEElementRef& rhs);

    private:
        StorageRef storage_;
    };

    /*explicit*/ inline BEElementRef::BEElementRef()
        : storage_()
    {
    }

    /*explicit*/ inline BEElementRef::BEElementRef(StorageRef&& storage)
        : storage_(std::move(storage))
    {
    }

    inline bool BEElementRef::is_valid() const
    {
        return (element_id() != ElementId::None);
    }

    inline ElementId BEElementRef::element_id() const
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

    inline bool BEElementRef::is_string() const
    {
        return (element_id() == ElementId::String);
    }

    inline const StringRef& BEElementRef::as_string() const
    {
        assert(is_string());
        return *std::get_if<ElementIdToIndex(ElementId::String)>(&storage_);
    }

    inline StringRef& BEElementRef::as_string()
    {
        return const_cast<StringRef&>(static_cast<const BEElementRef&>(*this).as_string());
    }

    inline bool BEElementRef::is_integer() const
    {
        return (element_id() == ElementId::Integer);
    }

    inline const IntegerRef& BEElementRef::as_integer() const
    {
        assert(is_integer());
        return *std::get_if<ElementIdToIndex(ElementId::Integer)>(&storage_);
    }

    inline IntegerRef& BEElementRef::as_integer()
    {
        return const_cast<IntegerRef&>(static_cast<const BEElementRef&>(*this).as_integer());
    }

    inline bool BEElementRef::is_list() const
    {
        return (element_id() == ElementId::List);
    }

    inline const ListRef& BEElementRef::as_list() const
    {
        assert(is_list());
        return *std::get_if<ElementIdToIndex(ElementId::List)>(&storage_);
    }

    inline ListRef& BEElementRef::as_list()
    {
        return const_cast<ListRef&>(static_cast<const BEElementRef&>(*this).as_list());
    }

    inline bool BEElementRef::is_dictionary() const
    {
        return (element_id() == ElementId::Dictionary);
    }

    inline const DictionaryRef& BEElementRef::as_dictionary() const
    {
        assert(is_dictionary());
        return *std::get_if<ElementIdToIndex(ElementId::Dictionary)>(&storage_);
    }

    inline DictionaryRef& BEElementRef::as_dictionary()
    {
        return const_cast<DictionaryRef&>(static_cast<const BEElementRef&>(*this).as_dictionary());
    }

    inline bool operator==(const BEElementRef& lhs, const BEElementRef& rhs)
    {
        return (lhs.storage_ == rhs.storage_);
    }

    inline bool operator!=(const BEElementRef& lhs, const BEElementRef& rhs)
    {
        return (lhs.storage_ != rhs.storage_);
    }

} // namespace be
