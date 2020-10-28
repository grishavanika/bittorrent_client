#pragma once
#include <bencoding/be_errors.h>

#include <map>
#include <vector>
#include <string_view>
#include <variant>
#include <utility>

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
    struct IntegerRef;
    struct StringRef;
    using ListRef        = std::vector<ElementRef>;
    using DictionaryRef  = std::vector<std::pair<StringRef, ElementRef>>;
    using StorageRef     = std::variant<IntegerRef, StringRef, ListRef, DictionaryRef>;

    struct IntegerRef : std::string_view
    {
        using std::string_view::string_view;
        static IntegerRef Make(std::string_view&& s)
        {
            IntegerRef self;
            static_cast<std::string_view&>(self) = std::move(s);
            return self;
        }
    };

    struct StringRef : std::string_view
    {
        using std::string_view::string_view;
        static StringRef Make(std::string_view&& s)
        {
            StringRef self;
            static_cast<std::string_view&>(self) = std::move(s);
            return self;
        }
    };

    // Index of ElementId in `StorageRef` variant.
    constexpr std::size_t ElementIdToIndex(ElementId id) noexcept
    {
        switch (id)
        {
        case ElementId::Integer:    return 0u;
        case ElementId::String:     return 1u;
        case ElementId::List:       return 2u;
        case ElementId::Dictionary: return 3u;
        case ElementId::None:       return std::size_t(-1);
        }
        return std::size_t(-1);
    }

    struct ElementPosition
    {
        std::size_t start_ = 0;
        std::size_t end_ = 0;
    };

    class ElementRef
    {
    public:
        explicit ElementRef(StorageRef&& storage, ElementPosition p);

    public:
        ElementId element_id() const;
        ElementPosition position() const;

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
        ElementPosition position_;
    };

    /*explicit*/ inline ElementRef::ElementRef(StorageRef&& storage, ElementPosition p)
        : storage_(std::move(storage))
        , position_(p)
    {
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

    inline ElementPosition ElementRef::position() const
    {
        return position_;
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
