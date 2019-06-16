#pragma once
#include <nonstd/expected.hpp>
#include <nonstd/string_view.hpp>
#include <nonstd/variant.hpp>

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

	class BEElementRef
	{
	public:
		using Integer = nonstd::string_view;
		using String = nonstd::string_view;
		using List = std::vector<BEElementRef>;
		using Dictionary = std::vector<std::pair<String, BEElementRef>>;
		using Storage = nonstd::variant<Integer, String, List, Dictionary>;

	public:
		explicit BEElementRef();
		explicit BEElementRef(Storage&& storage);

	public:
		ElementId element_id() const;

		bool is_valid() const;

		bool is_string() const;
		const String& as_string() const;
		String& as_string();

		bool is_integer() const;
		const String& as_integer() const;
		String& as_integer();

		bool is_list() const;
		const List& as_list() const;
		List& as_list();

		bool is_dictionary() const;
		const Dictionary& as_dictionary() const;
		Dictionary& as_dictionary();

	public:
		friend bool operator==(
			const BEElementRef& lhs, const BEElementRef& rhs);
		friend bool operator!=(
			const BEElementRef& lhs, const BEElementRef& rhs);

	private:
		Storage storage_;
	};

	using BEElementsArrayRef = std::vector<BEElementRef>;

	/*explicit*/ inline BEElementRef::BEElementRef()
		: storage_()
	{
	}

	/*explicit*/ inline BEElementRef::BEElementRef(Storage&& storage)
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
		case 0: return ElementId::Integer;
		case 1: return ElementId::String;
		case 2: return ElementId::List;
		case 3: return ElementId::Dictionary;
		}
		return ElementId::None;
	}

	inline bool BEElementRef::is_string() const
	{
		return (element_id() == ElementId::String);
	}

	inline const BEElementRef::String& BEElementRef::as_string() const
	{
		assert(is_string());
		return *nonstd::get_if<1>(&storage_);
	}

	inline BEElementRef::String& BEElementRef::as_string()
	{
		return const_cast<BEElementRef::String&>(
			static_cast<const BEElementRef&>(*this).as_string());
	}

	inline bool BEElementRef::is_integer() const
	{
		return (element_id() == ElementId::Integer);
	}

	inline const BEElementRef::String& BEElementRef::as_integer() const
	{
		assert(is_integer());
		return *nonstd::get_if<0>(&storage_);
	}

	inline BEElementRef::String& BEElementRef::as_integer()
	{
		return const_cast<BEElementRef::String&>(
			static_cast<const BEElementRef&>(*this).as_integer());
	}

	inline bool BEElementRef::is_list() const
	{
		return (element_id() == ElementId::List);
	}

	inline const BEElementRef::List& BEElementRef::as_list() const
	{
		assert(is_list());
		return *nonstd::get_if<2>(&storage_);
	}

	inline BEElementRef::List& BEElementRef::as_list()
	{
		return const_cast<BEElementRef::List&>(
			static_cast<const BEElementRef&>(*this).as_list());
	}

	inline bool BEElementRef::is_dictionary() const
	{
		return (element_id() == ElementId::Dictionary);
	}

	inline const BEElementRef::Dictionary& BEElementRef::as_dictionary() const
	{
		assert(is_dictionary());
		return *nonstd::get_if<3>(&storage_);
	}

	inline BEElementRef::Dictionary& BEElementRef::as_dictionary()
	{
		return const_cast<BEElementRef::Dictionary&>(
			static_cast<const BEElementRef&>(*this).as_dictionary());
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
