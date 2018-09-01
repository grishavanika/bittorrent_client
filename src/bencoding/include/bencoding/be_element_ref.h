#pragma once
#include <vector>
#include <map>
#include <utility>

#include <nonstd/variant.hpp>
#include <nonstd/string_view.hpp>
#include <nonstd/expected.hpp>

#include <cctype>
#include <cassert>

namespace be
{

	enum class ElementId
	{
		None,
		Number,
		String,
		List,
		Dictionary,
	};

	class BEElementRef
	{
	public:
		using Number     = nonstd::string_view;
		using String     = nonstd::string_view;
		using List       = std::vector<BEElementRef>;
		using Dictionary = std::vector<std::pair<String, BEElementRef>>;
		using Storage    = nonstd::variant<Number, String, List, Dictionary>;

	public:
		explicit BEElementRef();
		explicit BEElementRef(Storage&& storage);

	public:
		ElementId element_id() const;

		bool is_valid() const;

		bool is_string() const;
		const String& as_string() const;
		String& as_string();

		bool is_number() const;
		const String& as_number() const;
		String& as_number();

		bool is_list() const;
		const List& as_list() const;
		List& as_list();

	public:
		friend bool operator==(const BEElementRef& lhs, const BEElementRef& rhs);
		friend bool operator!=(const BEElementRef& lhs, const BEElementRef& rhs);

	private:
		Storage storage_;
	};

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
		case 0: return ElementId::Number;
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

	inline bool BEElementRef::is_number() const
	{
		return (element_id() == ElementId::Number);
	}

	inline const BEElementRef::String& BEElementRef::as_number() const
	{
		assert(is_number());
		return *nonstd::get_if<0>(&storage_);
	}

	inline BEElementRef::String& BEElementRef::as_number()
	{
		return const_cast<BEElementRef::String&>(
			static_cast<const BEElementRef&>(*this).as_number());
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

	inline bool operator==(const BEElementRef& lhs, const BEElementRef& rhs)
	{
		return (lhs.storage_ == rhs.storage_);
	}

	inline bool operator!=(const BEElementRef& lhs, const BEElementRef& rhs)
	{
		return (lhs.storage_ != rhs.storage_);
	}

} // namespace be
